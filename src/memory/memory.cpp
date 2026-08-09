#include "memory/memory.h"
#include "memory/memory_map.h"
#include "utils/log.h"
#include <algorithm>
#include <filesystem>
#include <fstream>

namespace N64 {
namespace Memory {

namespace {

namespace fs = std::filesystem;

// Make a ROM header title safe as a single path component.
std::string sanitize_game_folder_name(std::string name) {
    for (char &c : name) {
        const auto uc = static_cast<unsigned char>(c);
        if (uc < 0x20 || c == '/' || c == '\\' || c == ':' || c == '*' ||
            c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
            c = '_';
        }
    }
    while (!name.empty() &&
           (name.back() == ' ' || name.back() == '.' || name.back() == '_'))
        name.pop_back();
    const auto start = name.find_first_not_of(" ._");
    if (start == std::string::npos)
        return "unknown";
    if (start > 0)
        name = name.substr(start);
    return name.empty() ? "unknown" : name;
}

} // namespace

Memory::Memory() : rdram({}), sram({}) { rdram.assign(RDRAM_SIZE, 0); }

void Memory::reset() {
    Utils::debug("Resetting Memory (RDRAM)");
    ri.reset();
}

void Memory::set_data_dir(const std::string &dir) {
    data_dir = dir.empty() ? "." : dir;
}

std::string Memory::cart_save_path(const char *filename) const {
    std::string folder = sanitize_game_folder_name(rom.get_image_name());
    return (fs::path(data_dir) / "save" / folder / filename).string();
}

void Memory::load_rom(const std::string &rom_filepath) {
    rom.load_file(rom_filepath);
    allocate_sram();
    if (!sram.empty()) {
        sram_path = cart_save_path("save.sra");
        load_sram_file();
    } else {
        sram_path.clear();
    }
}

void Memory::allocate_sram() {
    if (rom.get_save_type() == SaveType::Sram256k) {
        sram.assign(SRAM_SIZE, 0xFF);
    } else {
        sram.clear();
    }
}

void Memory::load_sram_file() {
    if (sram.empty() || sram_path.empty())
        return;

    std::ifstream file(sram_path, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        Utils::info("No SRAM save yet: {}", sram_path);
        return;
    }

    file.read(reinterpret_cast<char *>(sram.data()),
              static_cast<std::streamsize>(sram.size()));
    const auto got = static_cast<size_t>(file.gcount());
    if (got < sram.size())
        std::fill(sram.begin() + static_cast<std::ptrdiff_t>(got), sram.end(),
                  0xFF);
    Utils::info("Loaded SRAM ({} bytes) from {}", got, sram_path);
}

void Memory::persist_sram() {
    if (sram.empty() || sram_path.empty())
        return;

    std::error_code ec;
    fs::create_directories(fs::path(sram_path).parent_path(), ec);
    if (ec) {
        Utils::warn("Failed to create save dir for {}: {}", sram_path,
                    ec.message());
        return;
    }

    std::ofstream file(sram_path,
                       std::ios::out | std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        Utils::warn("Failed to write SRAM: {}", sram_path);
        return;
    }
    file.write(reinterpret_cast<const char *>(sram.data()),
               static_cast<std::streamsize>(sram.size()));
    Utils::info("Saved SRAM ({} bytes) to {}", sram.size(), sram_path);
}

Memory &Memory::get_instance() { return instance; }

std::vector<uint8_t> &Memory::get_rdram() { return rdram; }

std::vector<uint8_t> &Memory::get_sram() { return sram; }

Memory Memory::instance{};

} // namespace Memory

Memory::Memory &g_memory() { return Memory::Memory::get_instance(); }

} // namespace N64
