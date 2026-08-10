# Generates about_info.h with the current git commit.
# Expected -D variables: SOURCE_DIR, OUTPUT_DIR

if(NOT SOURCE_DIR OR NOT OUTPUT_DIR)
  message(FATAL_ERROR "SOURCE_DIR and OUTPUT_DIR are required")
endif()

find_package(Git QUIET)
set(N64_EMU_GIT_HASH "unknown")
set(N64_EMU_GIT_HASH_FULL "unknown")
if(GIT_FOUND AND EXISTS "${SOURCE_DIR}/.git")
  execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
    WORKING_DIRECTORY "${SOURCE_DIR}"
    OUTPUT_VARIABLE N64_EMU_GIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
  )
  execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
    WORKING_DIRECTORY "${SOURCE_DIR}"
    OUTPUT_VARIABLE N64_EMU_GIT_HASH_FULL
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
  )
  execute_process(
    COMMAND ${GIT_EXECUTABLE} describe --always --dirty
    WORKING_DIRECTORY "${SOURCE_DIR}"
    OUTPUT_VARIABLE N64_EMU_GIT_DESCRIBE
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
  )
  if(N64_EMU_GIT_DESCRIBE)
    set(N64_EMU_GIT_HASH "${N64_EMU_GIT_DESCRIBE}")
  endif()
endif()

if(NOT N64_EMU_GIT_HASH)
  set(N64_EMU_GIT_HASH "unknown")
endif()
if(NOT N64_EMU_GIT_HASH_FULL)
  set(N64_EMU_GIT_HASH_FULL "unknown")
endif()

set(HEADER_CONTENT
"#pragma once

namespace N64 {
namespace Ui {

inline constexpr const char *kGitHash = \"${N64_EMU_GIT_HASH}\";
inline constexpr const char *kGitHashFull = \"${N64_EMU_GIT_HASH_FULL}\";

} // namespace Ui
} // namespace N64
")

file(MAKE_DIRECTORY "${OUTPUT_DIR}")
set(HEADER_TMP "${OUTPUT_DIR}/about_info.h.tmp")
file(WRITE "${HEADER_TMP}" "${HEADER_CONTENT}")
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different
  "${HEADER_TMP}" "${OUTPUT_DIR}/about_info.h")
file(REMOVE "${HEADER_TMP}")
