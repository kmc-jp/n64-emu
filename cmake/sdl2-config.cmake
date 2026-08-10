# https://trenki2.github.io/blog/2017/06/02/using-sdl2-with-cmake/
# https://discourse.libsdl.org/t/how-is-sdl2-supposed-to-be-used-with-cmake/31275
#
# Point SDL2_DIR at this cmake/ directory when using a Windows SDL2 layout with
# include/ and lib/ at the repository root (legacy submodule-style layout).
if(MSVC OR MSYS OR MINGW)
    message("Windows detected, using SDL2 from submodule")
    get_filename_component(_N64_SDL2_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
    set(SDL2_INCLUDE_DIRS "${_N64_SDL2_ROOT}/include")

    if(${CMAKE_SIZEOF_VOID_P} MATCHES 8)
        set(SDL2_LIBRARIES "${_N64_SDL2_ROOT}/lib/x64/SDL2.lib;${_N64_SDL2_ROOT}/lib/x64/SDL2main.lib")
    else()
        set(SDL2_LIBRARIES "${_N64_SDL2_ROOT}/lib/x86/SDL2.lib;${_N64_SDL2_ROOT}/lib/x86/SDL2main.lib")
    endif()

    string(STRIP "${SDL2_LIBRARIES}" SDL2_LIBRARIES)
    unset(_N64_SDL2_ROOT)
endif()
