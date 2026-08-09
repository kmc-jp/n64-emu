add_library(app STATIC)
target_sources(app PRIVATE
    app.cpp
    parallel_rdp_wrapper.cpp
    frame_interpolate.cpp
    depth_capture.cpp
)

# ---------------------------------------------------------------------------
# GLSL -> SPIR-V headers (requires glslc from Vulkan SDK)
# ---------------------------------------------------------------------------
set(N64_FRAME_INTERP OFF)

if(DEFINED ENV{VULKAN_SDK} AND EXISTS "$ENV{VULKAN_SDK}/Bin/glslc${CMAKE_EXECUTABLE_SUFFIX}")
    set(N64_GLSLC "$ENV{VULKAN_SDK}/Bin/glslc${CMAKE_EXECUTABLE_SUFFIX}")
elseif(DEFINED ENV{VULKAN_SDK} AND EXISTS "$ENV{VULKAN_SDK}/bin/glslc${CMAKE_EXECUTABLE_SUFFIX}")
    set(N64_GLSLC "$ENV{VULKAN_SDK}/bin/glslc${CMAKE_EXECUTABLE_SUFFIX}")
else()
    file(GLOB _n64_glslc_candidates
        "C:/VulkanSDK/*/Bin/glslc.exe"
        "/usr/bin/glslc"
        "/usr/local/bin/glslc"
    )
    if(_n64_glslc_candidates)
        list(GET _n64_glslc_candidates 0 N64_GLSLC)
    else()
        find_program(N64_GLSLC glslc)
    endif()
endif()

if(NOT N64_GLSLC)
    message(FATAL_ERROR
        "[n64-emu] glslc not found. Install the Vulkan SDK and ensure "
        "VULKAN_SDK is set (needed to compile src/app/shaders).")
endif()

set(_shader_gen_dir "${CMAKE_CURRENT_BINARY_DIR}/generated_shaders")
file(MAKE_DIRECTORY "${_shader_gen_dir}")
set(_shader_headers)

# relpath: path under shaders/ without extension, e.g. "blit" or "optical_flow/flow_refine"
function(n64_compile_shader relpath ext var_name)
    set(_src "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${relpath}.${ext}")
    set(_hdr "${_shader_gen_dir}/${var_name}.h")
    add_custom_command(
        OUTPUT "${_hdr}"
        COMMAND ${CMAKE_COMMAND}
            -DGLSLC=${N64_GLSLC}
            -DSHADER=${_src}
            -DOUT=${_hdr}
            -DVAR=${var_name}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/shaders/glsl_to_header.cmake"
        DEPENDS "${_src}" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/glsl_to_header.cmake"
        COMMENT "Compiling ${relpath}.${ext} -> SPIR-V header"
        VERBATIM
    )
    set(_acc ${_shader_headers})
    list(APPEND _acc "${_hdr}")
    set(_shader_headers "${_acc}" PARENT_SCOPE)
endfunction()

# Present blit
n64_compile_shader(fullscreen vert vertex_spirv)
n64_compile_shader(blit frag fragment_spirv)

# Optical-flow frame interpolation
set(_fi_shaders
    luma_downsample
    pyramid_down
    flow_refine
    flow_smooth
    flow_advect
    flow_consistency
    flow_temporal_blend
    flow_temporal_save
    flow_scale
    flow_global_reduce
    flow_global_finalize
    content_fingerprint
    content_compare
    content_finalize
    scene_reduce
    scene_finalize
    warp_blend
    depth_extract
)
foreach(_name IN LISTS _fi_shaders)
    n64_compile_shader(optical_flow/${_name} comp ${_name}_spirv)
endforeach()

add_custom_target(app_shaders DEPENDS ${_shader_headers})
target_sources(app PRIVATE ${_shader_headers})
add_dependencies(app app_shaders)
target_include_directories(app PRIVATE "${_shader_gen_dir}")

set(N64_FRAME_INTERP ON)
target_compile_definitions(app PUBLIC N64_FRAME_INTERP=1)
message(STATUS "[n64-emu] Shaders: glslc=${N64_GLSLC}")
message(STATUS "[n64-emu] Frame interpolation: ON")

target_link_libraries(app PUBLIC
    audio
    common
    log
    memory
    mmio
    n64_system
    parallel-rdp
    SDL2::SDL2
)
