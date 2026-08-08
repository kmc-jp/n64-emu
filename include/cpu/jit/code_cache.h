#ifndef CPU_JIT_CODE_CACHE_H
#define CPU_JIT_CODE_CACHE_H

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace N64 {
namespace Cpu {
namespace Jit {

using BlockFn = int (*)();

struct CompiledBlock {
    BlockFn fn{nullptr};
    uint32_t paddr{0};
    uint16_t num_insts{0};
};

class CodeCache {
  public:
    CodeCache();
    ~CodeCache();

    CodeCache(const CodeCache &) = delete;
    CodeCache &operator=(const CodeCache &) = delete;

    CompiledBlock *lookup(uint32_t paddr);
    void insert(uint32_t paddr, BlockFn fn, uint16_t num_insts);

    void invalidate_page(uint32_t paddr);
    void invalidate_range(uint32_t paddr, uint32_t length);
    void clear();

    // Bump-allocate executable bytes from a shared slab.
    uint8_t *alloc_exec(size_t size);
    // After emitting into a reservation, give back unused tail bytes.
    void shrink_last_alloc(size_t reserved, size_t used);

  private:
    static constexpr uint32_t PAGE_SHIFT = 12;
    static constexpr uint32_t PAGE_SIZE = 1u << PAGE_SHIFT;
    static constexpr uint32_t WORDS_PER_PAGE = PAGE_SIZE / 4;
    // Guard against unbounded growth when code is invalidated and recompiled.
    static constexpr size_t MAX_BLOCKS = 8192;
    static constexpr size_t SLAB_SIZE = 2 * 1024 * 1024;
    static constexpr size_t MAX_SLAB_BYTES = 32 * 1024 * 1024;

    struct Page {
        std::vector<CompiledBlock *> entries;
        bool has_code{false};
    };

    struct Slab {
        uint8_t *ptr;
        size_t bytes;
        size_t used;
    };

    void maybe_flush();

    std::unordered_map<uint32_t, std::unique_ptr<Page>> pages_;
    std::vector<std::unique_ptr<CompiledBlock>> blocks_;
    std::vector<Slab> slabs_;
    size_t total_slab_bytes_{0};
};

} // namespace Jit
} // namespace Cpu
} // namespace N64

#endif
