#add_library(imgui STATIC)
add_subdirectory(parallel-rdp-standalone)
add_subdirectory(spdlog)

#file(GLOB IMGUI_SOURCES CONFIGURE_DEPENDS imgui/*.cpp)
#target_sources(imgui PRIVATE
#    "${IMGUI_SOURCES}"
#)
#target_include_directories(imgui INTERFACE imgui)
#target_link_libraries(imgui PRIVATE
#    common
#    utils
#)

target_include_directories(spdlog SYSTEM INTERFACE
    spdlog/include
)
