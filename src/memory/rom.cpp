#include "rom.h"

namespace N64 {

void Memory::Rom::read_from(const std::string &filepath) {

    spdlog::debug("reading from ROM");

    std::ifstream file(filepath.c_str(), std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        spdlog::error("Could not open ROM file");
        return;
    }
    rom = {std::istreambuf_iterator<char>(file),
           std::istreambuf_iterator<char>()};
    // size of phisical memory region mapped to memory
    rom.resize(0xFC00000);

    if (rom.size() < sizeof(rom_header_t)) {
        spdlog::error("ROM is too small");
        return;
    }

    // set header
    header = *((rom_header_t *)rom.data());

    spdlog::debug("ROM size\t= {}", rom.size());
    spdlog::debug("imageName\t= \"{}\"", std::string(header.image_name));
    broken = false;
}

} // namespace N64