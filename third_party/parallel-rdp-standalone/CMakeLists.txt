file(GLOB_RECURSE parallel-rdp-cpp parallel-rdp/*.cpp)

add_library(parallel-rdp
    ${parallel-rdp-cpp}
    vulkan/buffer.cpp
    vulkan/buffer_pool.cpp
    vulkan/command_buffer.cpp
    vulkan/command_pool.cpp
    vulkan/context.cpp
    vulkan/cookie.cpp
    vulkan/descriptor_set.cpp
    vulkan/device.cpp
    vulkan/event_manager.cpp
    vulkan/fence.cpp
    vulkan/fence_manager.cpp
    vulkan/image.cpp
    vulkan/indirect_layout.cpp
    vulkan/memory_allocator.cpp
    vulkan/pipeline_event.cpp
    vulkan/query_pool.cpp
    vulkan/render_pass.cpp
    vulkan/sampler.cpp
    vulkan/semaphore.cpp
    vulkan/semaphore_manager.cpp
    vulkan/shader.cpp
    vulkan/texture/texture_format.cpp
    util/arena_allocator.cpp
    util/logging.cpp
    util/thread_id.cpp
    util/aligned_alloc.cpp
    util/timer.cpp
    util/timeline_trace_file.cpp
    util/thread_name.cpp
    volk/volk.c
)

# FIXME: set the following options
# PARALLEL_RDP_LDFLAGS := -pthread
# ifeq (,$(findstring win,$(platform)))
#     PARALLEL_RDP_LDFLAGS += -ldl
# else
#     PARALLEL_RDP_CFLAGS += -DVK_USE_PLATFORM_WIN32_KHR
#     PARALLEL_RDP_LDFLAGS += -lwinmm
# endif

if(WIN32)
    target_compile_definitions(parallel-rdp PUBLIC VK_USE_PLATFORM_WIN32_KHR)
    target_link_libraries(parallel-rdp PUBLIC winmm)
    if(MSVC)
        target_compile_options(parallel-rdp PUBLIC /EHa)
    endif()
else()
    target_link_libraries(parallel-rdp PUBLIC dl)
endif()

# TODO: 

target_include_directories(parallel-rdp PUBLIC
    vulkan-headers/include
    parallel-rdp
    volk
    spirv-cross
    vulkan
    util
)
