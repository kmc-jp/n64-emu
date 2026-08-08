#include "rcp/jit/code_cache.h"
#include "utils/log.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif
#include <cstring>

namespace N64 {
namespace Rsp {
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

CodeCache::CodeCache() { by_pc_.fill(nullptr); }

CodeCache::~CodeCache() { release_all(); }

void CodeCache::maybe_flush() {
    if (blocks_.size() > MAX_BLOCKS || total_slab_bytes_ > MAX_SLAB_BYTES) {
        Utils::debug("RSP JIT: flushing code cache (blocks={} bytes={:#x})",
                     blocks_.size(), total_slab_bytes_);
        release_all();
    }
}

uint8_t *CodeCache::alloc_exec(size_t size) {
    maybe_flush();
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
        Utils::critical("RSP JIT: executable memory allocation failed");
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
    if (used > reserved || reserved > cur.used)
        return;
    cur.used -= (reserved - used);
}

CompiledBlock *CodeCache::lookup(uint16_t pc) {
    const uint32_t idx = (pc >> 2) & (IMEM_WORDS - 1);
    CompiledBlock *b = by_pc_[idx];
    if (b && b->pc == (pc & 0xFFC)) {
        last_hit_ = b;
        return b;
    }
    if (last_hit_ && last_hit_->pc == (pc & 0xFFC))
        return last_hit_;
    return nullptr;
}

void CodeCache::insert(uint16_t pc, BlockFn fn, uint16_t num_insts) {
    maybe_flush();
    auto block = std::make_unique<CompiledBlock>();
    block->fn = fn;
    block->pc = pc & 0xFFC;
    block->num_insts = num_insts;
    const uint32_t idx = (block->pc >> 2) & (IMEM_WORDS - 1);
    by_pc_[idx] = block.get();
    last_hit_ = block.get();
    blocks_.push_back(std::move(block));
}

void CodeCache::invalidate_all() {
    // Soft invalidate only: drop lookup entries so the next run recompiles.
    // Never munmap here — RSP DMA / CPU IMEM writes can hit while a block is
    // still executing (same rule as the CPU JIT page invalidation path).
    by_pc_.fill(nullptr);
    last_hit_ = nullptr;
    ++generation_;
}

void CodeCache::release_all() {
    by_pc_.fill(nullptr);
    last_hit_ = nullptr;
    ++generation_;
    blocks_.clear();
    for (auto &s : slabs_)
        free_rwx(s.ptr, s.bytes);
    slabs_.clear();
    total_slab_bytes_ = 0;
}

void CodeCache::invalidate_range(uint16_t addr, uint16_t length) {
    if (length == 0)
        return;
    // IMEM is tiny; a full soft flush keeps correctness simple and matches how
    // often ucode overlays are replaced wholesale.
    (void)addr;
    invalidate_all();
}

} // namespace Jit
} // namespace Rsp
} // namespace N64
