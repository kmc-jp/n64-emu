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

// Comparison key: preferred separators + case-folded on Windows so that
// C:/a/b.z64 and C:\a\b.z64 (and mixed casing) map to the same entry.
std::string path_key(const std::string &path) {
    std::string key = normalize_path(path);
#ifdef _WIN32
    for (char &c : key) {
        if (c == '/')
            c = '\\';
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
#endif
    return key;
}

bool path_equal(const std::string &a, const std::string &b) {
    return path_key(a) == path_key(b);
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

    bool changed = false;
    std::string line;
    while (std::getline(in, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
            line.pop_back();
        if (line.empty())
            continue;
        const std::string normalized = normalize_path(line);
        if (normalized != line)
            changed = true;
        if (std::any_of(out.begin(), out.end(), [&](const std::string &p) {
                return path_equal(p, normalized);
            })) {
            changed = true;
            continue;
        }
        out.push_back(normalized);
        if (out.size() >= kMaxRecentRoms)
            break;
    }
    in.close();
    if (changed)
        save_recent_roms(out);
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
                                   return path_equal(p, normalized);
                               }),
                paths.end());
    paths.insert(paths.begin(), normalized);
    if (paths.size() > kMaxRecentRoms)
        paths.resize(kMaxRecentRoms);
    save_recent_roms(paths);
}

} // namespace Ui
} // namespace N64
