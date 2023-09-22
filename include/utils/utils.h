#ifndef UTILS_H
#define UTILS_H

#include <cstdint>
#include <string>
#include <vector>

namespace Utils {
std::vector<uint8_t> read_binary_file(std::string filepath);
} // namespace Utils

#endif
