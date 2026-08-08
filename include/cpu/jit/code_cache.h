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

    uint8_t *alloc_exec(size_t size);

  private:
    static constexpr uint32_t PAGE_SHIFT = 12;
    static constexpr uint32_t PAGE_SIZE = 1u << PAGE_SHIFT;
    static constexpr uint32_t WORDS_PER_PAGE = PAGE_SIZE / 4;

    struct Page {
        std::vector<CompiledBlock *> entries;
        bool has_code{false};
    };

    struct Slab {
        uint8_t *ptr;
        size_t bytes;
    };

    std::unordered_map<uint32_t, std::unique_ptr<Page>> pages_;
    std::vector<std::unique_ptr<CompiledBlock>> blocks_;
    std::vector<Slab> slabs_;
};

} // namespace Jit
} // namespace Cpu
} // namespace N64

#endif
