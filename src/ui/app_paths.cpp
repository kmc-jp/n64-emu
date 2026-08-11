#include "ui/app_paths.h"
#include "app_identity.h"
#include <SDL.h>
#include <filesystem>
#include <system_error>

namespace N64 {
namespace Ui {

namespace {

namespace fs = std::filesystem;

// SDL_GetPrefPath always ends with a separator. parent_path() on "a/b/" is
// "a/b", not "a" — strip empty filename components first.
fs::path strip_trailing_separators(fs::path p) {
    while (!p.empty() && p.filename().empty())
        p = p.parent_path();
    return p;
}

void migrate_nested_contents(const fs::path &nested, const fs::path &dir) {
    std::error_code ec;
    if (!fs::is_directory(nested, ec))
        return;
    for (const auto &entry : fs::directory_iterator(nested, ec)) {
        if (ec)
            break;
        const fs::path dest = dir / entry.path().filename();
        if (fs::exists(dest, ec))
            continue;
        fs::rename(entry.path(), dest, ec);
    }
    fs::remove(nested, ec);
}

} // namespace

std::string app_data_dir() {
    if (char *pref = SDL_GetPrefPath(kAppSlug, kAppSlug)) {
        // .../kamo64/kamo64/  ->  .../kamo64/kamo64  ->  .../kamo64
        const fs::path nested = strip_trailing_separators(fs::path(pref));
        SDL_free(pref);
        const fs::path dir = nested.parent_path();
        std::error_code ec;
        fs::create_directories(dir, ec);
        migrate_nested_contents(nested, dir);
        return dir.string();
    }
    return {};
}

} // namespace Ui
} // namespace N64
