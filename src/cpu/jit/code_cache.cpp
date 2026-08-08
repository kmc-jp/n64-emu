#include "cpu/jit/code_cache.h"
#include "utils/log.h"
#include <sys/mman.h>
#include <unistd.h>

namespace N64 {
namespace Cpu {
namespace Jit {

CodeCache::CodeCache() = default;

CodeCache::~CodeCache() { clear(); }

void CodeCache::maybe_flush() {
    if (blocks_.size() > MAX_BLOCKS || total_slab_bytes_ > MAX_SLAB_BYTES) {
        Utils::info("JIT: flushing code cache (blocks={} slabs_bytes={:#x})",
                    blocks_.size(), total_slab_bytes_);
        clear();
    }
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

    const size_t page = static_cast<size_t>(sysconf(_SC_PAGESIZE));
    size_t total = SLAB_SIZE;
    if (size + align > total)
        total = ((size + align + page - 1) / page) * page;

    void *mem = mmap(nullptr, total, PROT_READ | PROT_WRITE | PROT_EXEC,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) {
        Utils::critical("JIT: mmap executable memory failed");
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
    const uint32_t page = paddr >> PAGE_SHIFT;
    const uint32_t word = (paddr & (PAGE_SIZE - 1)) >> 2;
    auto it = pages_.find(page);
    if (it == pages_.end())
        return nullptr;
    auto &entries = it->second->entries;
    if (word >= entries.size())
        return nullptr;
    return entries[word];
}

void CodeCache::insert(uint32_t paddr, BlockFn fn, uint16_t num_insts) {
    const uint32_t page_idx = paddr >> PAGE_SHIFT;
    const uint32_t word = (paddr & (PAGE_SIZE - 1)) >> 2;

    auto &page = pages_[page_idx];
    if (!page) {
        page = std::make_unique<Page>();
        page->entries.assign(WORDS_PER_PAGE, nullptr);
    }
    auto block = std::make_unique<CompiledBlock>();
    block->fn = fn;
    block->paddr = paddr;
    block->num_insts = num_insts;
    CompiledBlock *raw = block.get();
    blocks_.push_back(std::move(block));
    page->entries[word] = raw;
    page->has_code = true;
}

void CodeCache::invalidate_page(uint32_t paddr) {
    const uint32_t page_idx = paddr >> PAGE_SHIFT;
    auto it = pages_.find(page_idx);
    if (it == pages_.end())
        return;
    for (auto *&e : it->second->entries)
        e = nullptr;
    it->second->has_code = false;
}

void CodeCache::invalidate_range(uint32_t paddr, uint32_t length) {
    if (length == 0)
        return;
    const uint32_t start = paddr & ~(PAGE_SIZE - 1);
    const uint64_t end64 =
        static_cast<uint64_t>(paddr) + static_cast<uint64_t>(length) - 1;
    const uint32_t end = end64 > 0xffffffffu ? 0xffffffffu
                                             : static_cast<uint32_t>(end64);
    for (uint64_t p = start; p <= end; p += PAGE_SIZE)
        invalidate_page(static_cast<uint32_t>(p));
}

void CodeCache::clear() {
    pages_.clear();
    blocks_.clear();
    for (auto &s : slabs_) {
        if (s.ptr)
            munmap(s.ptr, s.bytes);
    }
    slabs_.clear();
    total_slab_bytes_ = 0;
}

} // namespace Jit
} // namespace Cpu
} // namespace N64
