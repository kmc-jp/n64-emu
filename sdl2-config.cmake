# https://trenki2.github.io/blog/2017/06/02/using-sdl2-with-cmake/
# https://discourse.libsdl.org/t/how-is-sdl2-supposed-to-be-used-with-cmake/31275
if(MSVC OR MSYS OR MINGW)
    message("Windows detected, using SDL2 from submodule")
    set(SDL2_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/include")

    if(${CMAKE_SIZEOF_VOID_P} MATCHES 8)
        set(SDL2_LIBRARIES "${CMAKE_CURRENT_LIST_DIR}/lib/x64/SDL2.lib;${CMAKE_CURRENT_LIST_DIR}/lib/x64/SDL2main.lib")
    else()
        set(SDL2_LIBRARIES "${CMAKE_CURRENT_LIST_DIR}/lib/x86/SDL2.lib;${CMAKE_CURRENT_LIST_DIR}/lib/x86/SDL2main.lib")
    endif()

    string(STRIP "${SDL2_LIBRARIES}" SDL2_LIBRARIES)
endif()
