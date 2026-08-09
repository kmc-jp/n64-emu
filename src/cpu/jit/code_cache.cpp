#include "cpu/jit/code_cache.h"
#include "utils/log.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace N64 {
namespace Cpu {
namespace Jit {

namespace {
size_t host_page_size() {
#ifdef _WIN32
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return static_cast<size_t>(info.dwPageSize);
#else
    return static_cast<size_t>(sysconf(_SC_PAGESIZE));
#endif
}

void *alloc_rwx(size_t bytes) {
#ifdef _WIN32
    return VirtualAlloc(nullptr, bytes, MEM_COMMIT | MEM_RESERVE,
                        PAGE_EXECUTE_READWRITE);
#else
    void *mem = mmap(nullptr, bytes, PROT_READ | PROT_WRITE | PROT_EXEC,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return mem == MAP_FAILED ? nullptr : mem;
#endif
}

void free_rwx(void *ptr, size_t bytes) {
#ifdef _WIN32
    (void)bytes;
    VirtualFree(ptr, 0, MEM_RELEASE);
#else
    munmap(ptr, bytes);
#endif
}
} // namespace

CodeCache::CodeCache() { rdram_pages_.fill(nullptr); }

CodeCache::~CodeCache() { clear(); }

void CodeCache::maybe_flush() {
    if (blocks_.size() > MAX_BLOCKS || total_slab_bytes_ > MAX_SLAB_BYTES) {
        Utils::debug("JIT: flushing code cache (blocks={} slabs_bytes={:#x})",
                     blocks_.size(), total_slab_bytes_);
        clear();
    }
}

CodeCache::Page *CodeCache::find_page(uint32_t page_idx) const {
    if (page_idx < RDRAM_PAGES)
        return rdram_pages_[page_idx];
    auto it = other_pages_.find(page_idx);
    if (it == other_pages_.end())
        return nullptr;
    return it->second.get();
}

CodeCache::Page *CodeCache::get_or_create_page(uint32_t page_idx) {
    if (page_idx < RDRAM_PAGES) {
        Page *&slot = rdram_pages_[page_idx];
        if (!slot) {
            auto page = std::make_unique<Page>();
            page->entries.assign(WORDS_PER_PAGE, nullptr);
            slot = page.get();
            page_storage_.push_back(std::move(page));
        }
        return slot;
    }
    auto &page = other_pages_[page_idx];
    if (!page) {
        page = std::make_unique<Page>();
        page->entries.assign(WORDS_PER_PAGE, nullptr);
    }
    return page.get();
}

uint8_t *CodeCache::alloc_exec(size_t size) {
    maybe_flush();

    // Align to 16 for x86 call targets / Xbyak.
    const size_t align = 16;
    size = (size + align - 1) & ~(align - 1);

    if (!slabs_.empty()) {
        Slab &cur = slabs_.back();
        const size_t aligned_used = (cur.used + align - 1) & ~(align - 1);
        if (aligned_used + size <= cur.bytes) {
            cur.used = aligned_used + size;
            return cur.ptr + aligned_used;
        }
    }

    const size_t page = host_page_size();
    size_t total = SLAB_SIZE;
    if (size + align > total)
        total = ((size + align + page - 1) / page) * page;

    void *mem = alloc_rwx(total);
    if (!mem) {
        Utils::critical("JIT: executable memory allocation failed");
        Utils::abort("Aborted");
    }
    auto *p = static_cast<uint8_t *>(mem);
    slabs_.push_back(Slab{p, total, size});
    total_slab_bytes_ += total;
    return p;
}

void CodeCache::shrink_last_alloc(size_t reserved, size_t used) {
    if (slabs_.empty())
        return;
    Slab &cur = slabs_.back();
    if (cur.used < reserved)
        return;
    const size_t align = 16;
    const size_t used_aligned = (used + align - 1) & ~(align - 1);
    if (used_aligned > reserved)
        return;
    cur.used = cur.used - reserved + used_aligned;
}

CompiledBlock *CodeCache::lookup(uint32_t paddr) {
    if (last_hit_ && last_hit_->paddr == paddr)
        return last_hit_;

    const uint32_t page_idx = paddr >> PAGE_SHIFT;
    const uint32_t word = (paddr & (PAGE_SIZE - 1)) >> 2;
    Page *page = find_page(page_idx);
    if (!page)
        return nullptr;
    CompiledBlock *b = page->entries[word];
    if (b)
        last_hit_ = b;
    return b;
}

bool CodeCache::page_has_code(uint32_t paddr) const {
    Page *page = find_page(paddr >> PAGE_SHIFT);
    return page && page->has_code;
}

void CodeCache::insert(uint32_t paddr, BlockFn fn, uint16_t num_insts) {
    const uint32_t page_idx = paddr >> PAGE_SHIFT;
    const uint32_t word = (paddr & (PAGE_SIZE - 1)) >> 2;

    Page *page = get_or_create_page(page_idx);
    auto block = std::make_unique<CompiledBlock>();
    block->fn = fn;
    block->paddr = paddr;
    block->num_insts = num_insts;
    CompiledBlock *raw = block.get();
    blocks_.push_back(std::move(block));
    page->entries[word] = raw;
    page->has_code = true;
    last_hit_ = raw;
}

void CodeCache::invalidate_page(uint32_t paddr) {
    const uint32_t page_idx = paddr >> PAGE_SHIFT;
    Page *page = find_page(page_idx);
    // Data writes (framebuffer etc.) must not touch the lookup hint or we
    // destroy soft-chaining after every store.
    if (!page || !page->has_code)
        return;
    clear_lookup_hint();
    for (auto *&e : page->entries)
        e = nullptr;
    page->has_code = false;
}

void CodeCache::invalidate_range(uint32_t paddr, uint32_t length) {
    if (length == 0)
        return;
    const uint32_t start = paddr & ~(PAGE_SIZE - 1);
    const uint64_t end64 =
        static_cast<uint64_t>(paddr) + static_cast<uint64_t>(length) - 1;
    const uint32_t end = end64 > 0xffffffffu ? 0xffffffffu
                                             : static_cast<uint32_t>(end64);
    bool any = false;
    for (uint64_t p = start; p <= end; p += PAGE_SIZE) {
        Page *page = find_page(static_cast<uint32_t>(p) >> PAGE_SHIFT);
        if (!page || !page->has_code)
            continue;
        if (!any) {
            clear_lookup_hint();
            any = true;
        }
        for (auto *&e : page->entries)
            e = nullptr;
        page->has_code = false;
    }
}

void CodeCache::clear() {
    clear_lookup_hint();
    rdram_pages_.fill(nullptr);
    other_pages_.clear();
    page_storage_.clear();
    blocks_.clear();
    for (auto &s : slabs_) {
        if (s.ptr)
            free_rwx(s.ptr, s.bytes);
    }
    slabs_.clear();
    total_slab_bytes_ = 0;
}

} // namespace Jit
} // namespace Cpu
} // namespace N64
