#include <windows.h>
#include <commdlg.h>

#include "ui/win32_file_dialog.h"
#include "ui/config_toml.h"
#include "ui/gui.h"
#include <string>

#pragma comment(lib, "comdlg32.lib")

namespace N64 {
namespace Ui {

namespace {

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

} // namespace

bool win32_open_rom_dialog(GuiState &state) {
    if (!state.config || !state.ui_settings)
        return false;

    wchar_t file[MAX_PATH]{};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
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

    state.config->rom_filepath = wide_to_utf8(file);
    std::wstring path(file);
    const auto slash = path.find_last_of(L"\\/");
    if (slash != std::wstring::npos)
        state.ui_settings->last_rom_dir = wide_to_utf8(path.substr(0, slash));
    save_toml(*state.config, *state.ui_settings);

    state.request_start = true;
    if (state.mode == AppMode::Running)
        state.request_stop = true;
    return true;
}

} // namespace Ui
} // namespace N64
