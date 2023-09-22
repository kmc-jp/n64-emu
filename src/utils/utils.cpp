#include "utils/utils.h"
#include "utils/log.h"
#include <fstream>

namespace Utils {

std::vector<uint8_t> read_binary_file(std::string filepath) {
    std::ifstream file(filepath.c_str(), std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        Utils::abort("Could not open binary file: {}", filepath);
        exit(-1);
    }
    // determine file size
    uint64_t file_size = file.tellg();
    // go to the last byte
    file.seekg(0, std::ios::end);
    file_size = static_cast<uint64_t>(file.tellg()) - file_size;
    // go back to the first byte
    file.clear();
    file.seekg(0);

    std::vector<uint8_t> ret;
    ret.assign(file_size, 0);

    // Read entire ROM data
    file.read(reinterpret_cast<char *>(ret.data()), file_size);

    return ret;
}

} // namespace Utils
