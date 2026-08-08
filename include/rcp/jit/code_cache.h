#ifndef RCP_JIT_CODE_CACHE_H
#define RCP_JIT_CODE_CACHE_H

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace N64 {
namespace Rsp {
namespace Jit {

using BlockFn = int (*)();

struct CompiledBlock {
    BlockFn fn{nullptr};
    uint16_t pc{0};
    uint16_t num_insts{0};
};

// IMEM is a fixed 4KiB instruction space (1024 words). Lookup is a direct
// array index by PC word — same idea as the CPU cache, without RDRAM paging.
class CodeCache {
  public:
    CodeCache();
    ~CodeCache();

    CodeCache(const CodeCache &) = delete;
    CodeCache &operator=(const CodeCache &) = delete;

    CompiledBlock *lookup(uint16_t pc);
    void insert(uint16_t pc, BlockFn fn, uint16_t num_insts);

    void invalidate_all();
    void invalidate_range(uint16_t addr, uint16_t length);
    void clear() { release_all(); }

    // Bumped on every soft/hard invalidate so an in-flight block can stop
    // after the instruction that replaced IMEM.
    uint32_t generation() const { return generation_; }

    uint8_t *alloc_exec(size_t size);
    void shrink_last_alloc(size_t reserved, size_t used);

  private:
    static constexpr uint32_t IMEM_WORDS = 1024;
    static constexpr size_t MAX_BLOCKS = 2048;
    static constexpr size_t SLAB_SIZE = 512 * 1024;
    static constexpr size_t MAX_SLAB_BYTES = 8 * 1024 * 1024;

    struct Slab {
        uint8_t *ptr;
        size_t bytes;
        size_t used;
    };

    void maybe_flush();
    void release_all();

    std::array<CompiledBlock *, IMEM_WORDS> by_pc_{};
    std::vector<std::unique_ptr<CompiledBlock>> blocks_;
    std::vector<Slab> slabs_;
    size_t total_slab_bytes_{0};
    CompiledBlock *last_hit_{nullptr};
    uint32_t generation_{1};
};

} // namespace Jit
} // namespace Rsp
} // namespace N64

#endif
