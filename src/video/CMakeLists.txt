# Frame interpolation (optical flow) and depth capture for VI scanout.
add_library(video STATIC)
target_sources(video PRIVATE
    depth_capture.cpp
    frame_interpolate.cpp
)

# ---------------------------------------------------------------------------
# GLSL -> SPIR-V headers (requires glslc from Vulkan SDK)
# ---------------------------------------------------------------------------
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
        "VULKAN_SDK is set (needed to compile src/video/shaders).")
endif()

set(_shader_gen_dir "${CMAKE_CURRENT_BINARY_DIR}/generated_shaders")
file(MAKE_DIRECTORY "${_shader_gen_dir}")
set(_shader_headers)

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

add_custom_target(video_shaders DEPENDS ${_shader_headers})
target_sources(video PRIVATE ${_shader_headers})
add_dependencies(video video_shaders)
target_include_directories(video PRIVATE "${_shader_gen_dir}")

target_compile_definitions(video PUBLIC N64_FRAME_INTERP=1)
message(STATUS "[n64-emu] Frame-interp shaders: glslc=${N64_GLSLC}")

target_link_libraries(video PUBLIC
    common
    log
    parallel-rdp
)
