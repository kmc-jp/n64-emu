#include "ui/file_dialog.h"
#include "ui/config_toml.h"
#include "ui/gui.h"
#include "ui/sdl_platform.h"
#include "utils/log.h"
#include <SDL.h>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#include <SDL_syswm.h>
#pragma comment(lib, "comdlg32.lib")
#else
#include <array>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace N64 {
namespace Ui {

namespace {

namespace fs = std::filesystem;

#ifdef _WIN32

std::wstring utf8_to_wide(const std::string &s) {
    if (s.empty())
        return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring out(static_cast<size_t>(n ? n - 1 : 0), L'\0');
    if (n > 1)
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, out.data(), n);
    return out;
}

std::string wide_to_utf8(const std::wstring &s) {
    if (s.empty())
        return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, nullptr, 0,
                                      nullptr, nullptr);
    std::string out(static_cast<size_t>(n ? n - 1 : 0), '\0');
    if (n > 1)
        WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, out.data(), n, nullptr,
                            nullptr);
    return out;
}

HWND sdl_window_hwnd(SDL_Window *window) {
    if (!window)
        return nullptr;
    SDL_SysWMinfo info{};
    SDL_VERSION(&info.version);
    if (!SDL_GetWindowWMInfo(window, &info))
        return nullptr;
    return info.info.win.window;
}

bool pick_rom_path(GuiState &state, std::string &out_path) {
    SDL_Window *window = nullptr;
    if (state.wsi)
        window =
            static_cast<SDL2Platform &>(state.wsi->get_platform()).get_window();

    wchar_t file[MAX_PATH]{};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = sdl_window_hwnd(window);
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"N64 ROM (*.z64)\0*.z64\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    std::wstring initial = utf8_to_wide(state.ui_settings->last_rom_dir);
    if (!initial.empty())
        ofn.lpstrInitialDir = initial.c_str();

    if (!GetOpenFileNameW(&ofn))
        return false;
    out_path = wide_to_utf8(file);
    return !out_path.empty();
}

#else // !_WIN32

// Run argv (argv[0]=program) and capture stdout. Returns false on spawn/fail.
bool run_capture_stdout(const std::vector<std::string> &argv, std::string &out) {
    if (argv.empty())
        return false;

    int pipefd[2];
    if (pipe(pipefd) != 0)
        return false;

    const pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return false;
    }

    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        // Hide zenity/osascript stderr noise.
        freopen("/dev/null", "w", stderr);

        std::vector<char *> args;
        args.reserve(argv.size() + 1);
        for (const auto &s : argv)
            args.push_back(const_cast<char *>(s.c_str()));
        args.push_back(nullptr);
        execvp(args[0], args.data());
        _exit(127);
    }

    close(pipefd[1]);
    out.clear();
    std::array<char, 512> buf{};
    while (true) {
        const ssize_t n = read(pipefd[0], buf.data(), buf.size());
        if (n > 0)
            out.append(buf.data(), static_cast<size_t>(n));
        else
            break;
    }
    close(pipefd[0]);

    int status = 0;
    if (waitpid(pid, &status, 0) < 0)
        return false;
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
        return false;

    while (!out.empty() && (out.back() == '\n' || out.back() == '\r'))
        out.pop_back();
    return !out.empty();
}

std::string applescript_escape(const std::string &s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '\\' || c == '"')
            out.push_back('\\');
        out.push_back(c);
    }
    return out;
}

bool pick_rom_path_macos(const std::string &initial_dir, std::string &out_path) {
    // osascript has no reliable "*.z64" filter; accept any file.
    std::string script =
        "try\n"
        "  set theFile to choose file with prompt \"Open N64 ROM\"";
    if (!initial_dir.empty()) {
        script += " default location (POSIX file \"";
        script += applescript_escape(initial_dir);
        script += "\")";
    }
    script +=
        "\n  return POSIX path of theFile\n"
        "on error\n"
        "  return \"\"\n"
        "end try\n";

    std::string result;
    if (!run_capture_stdout({"osascript", "-e", script}, result))
        return false;
    out_path = std::move(result);
    return !out_path.empty();
}

bool pick_rom_path_linux(const std::string &initial_dir, std::string &out_path) {
    // Prefer zenity, then kdialog.
    {
        std::vector<std::string> args = {
            "zenity", "--file-selection", "--title=Open N64 ROM",
            "--file-filter=N64 ROM | *.z64", "--file-filter=All files | *"};
        if (!initial_dir.empty()) {
            fs::path start = fs::path(initial_dir) / "";
            args.push_back("--filename=" + start.string());
        }
        if (run_capture_stdout(args, out_path))
            return true;
    }
    {
        std::vector<std::string> args = {
            "kdialog", "--getopenfilename",
            initial_dir.empty() ? std::string(".") : initial_dir,
            "N64 ROM (*.z64)|All files (*)"};
        if (run_capture_stdout(args, out_path))
            return true;
    }

    Utils::warn(
        "No file dialog helper found (install zenity or kdialog), "
        "or pass a ROM path on the command line / use Recents");
    return false;
}

bool pick_rom_path(GuiState &state, std::string &out_path) {
    const std::string &initial = state.ui_settings->last_rom_dir;
#if defined(__APPLE__)
    return pick_rom_path_macos(initial, out_path);
#else
    return pick_rom_path_linux(initial, out_path);
#endif
}

#endif // _WIN32

} // namespace

bool is_n64_rom_path(const std::string &path) {
    if (path.empty())
        return false;
    std::string ext = std::filesystem::path(path).extension().string();
    for (char &c : ext) {
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
    }
    return ext == ".z64" || ext == ".n64" || ext == ".v64";
}

bool open_rom_file(GuiState &state, const std::string &path) {
    if (!state.config || path.empty())
        return false;

    state.config->rom_filepath = path;
    if (state.ui_settings) {
        const std::filesystem::path dir =
            std::filesystem::path(path).parent_path();
        if (!dir.empty()) {
            state.ui_settings->last_rom_dir = dir.string();
            save_toml(*state.config, *state.ui_settings);
        }
    }

    state.request_start = true;
    if (state.mode == AppMode::Running)
        state.request_stop = true;
    return true;
}

bool open_rom_dialog(GuiState &state) {
    if (!state.config || !state.ui_settings)
        return false;

    std::string path;
    if (!pick_rom_path(state, path))
        return false;
    return open_rom_file(state, path);
}

} // namespace Ui
} // namespace N64
