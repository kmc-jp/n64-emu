#include "ui/recent_roms.h"
#include "ui/config_toml.h"
#include "utils/log.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>

namespace N64 {
namespace Ui {

namespace {

namespace fs = std::filesystem;

std::string normalize_path(const std::string &path) {
    std::error_code ec;
    fs::path p = fs::weakly_canonical(fs::path(path), ec);
    if (ec)
        p = fs::path(path);
    return p.make_preferred().string();
}

bool path_equal(const std::string &a, const std::string &b) {
#ifdef _WIN32
    if (a.size() != b.size())
        return false;
    for (size_t i = 0; i < a.size(); ++i) {
        const auto ca =
            static_cast<char>(std::tolower(static_cast<unsigned char>(a[i])));
        const auto cb =
            static_cast<char>(std::tolower(static_cast<unsigned char>(b[i])));
        if (ca != cb)
            return false;
    }
    return true;
#else
    return a == b;
#endif
}

} // namespace

std::string recent_roms_path() {
    return (fs::path(settings_dir_path()) / "recent_roms.txt").string();
}

std::vector<std::string> load_recent_roms() {
    std::vector<std::string> out;
    std::ifstream in(recent_roms_path());
    if (!in)
        return out;

    std::string line;
    while (std::getline(in, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
            line.pop_back();
        if (line.empty())
            continue;
        if (std::any_of(out.begin(), out.end(), [&](const std::string &p) {
                return path_equal(p, line);
            }))
            continue;
        out.push_back(std::move(line));
        if (out.size() >= kMaxRecentRoms)
            break;
    }
    return out;
}

bool save_recent_roms(const std::vector<std::string> &paths) {
    const std::string path = recent_roms_path();
    std::error_code ec;
    fs::create_directories(fs::path(path).parent_path(), ec);
    std::ofstream out(path, std::ios::trunc);
    if (!out) {
        Utils::warn("Failed to write {}", path);
        return false;
    }
    const size_t n = std::min(paths.size(), kMaxRecentRoms);
    for (size_t i = 0; i < n; ++i)
        out << paths[i] << '\n';
    return true;
}

void remember_recent_rom(std::vector<std::string> &paths,
                         const std::string &path) {
    if (path.empty())
        return;
    const std::string normalized = normalize_path(path);
    paths.erase(std::remove_if(paths.begin(), paths.end(),
                               [&](const std::string &p) {
                                   return path_equal(p, normalized) ||
                                          path_equal(p, path);
                               }),
                paths.end());
    paths.insert(paths.begin(), normalized);
    if (paths.size() > kMaxRecentRoms)
        paths.resize(kMaxRecentRoms);
    save_recent_roms(paths);
}

} // namespace Ui
} // namespace N64
