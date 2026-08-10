# Generates app_identity.h with app identity constants and the current git commit.
# Expected -D variables: SOURCE_DIR, OUTPUT_DIR
# Identity values come from cmake/AppIdentity.cmake (included below).

if(NOT SOURCE_DIR OR NOT OUTPUT_DIR)
  message(FATAL_ERROR "SOURCE_DIR and OUTPUT_DIR are required")
endif()

include("${SOURCE_DIR}/cmake/AppIdentity.cmake")

find_package(Git QUIET)
set(APPID_GIT_HASH "unknown")
set(APPID_GIT_HASH_FULL "unknown")
if(GIT_FOUND AND EXISTS "${SOURCE_DIR}/.git")
  execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
    WORKING_DIRECTORY "${SOURCE_DIR}"
    OUTPUT_VARIABLE APPID_GIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
  )
  execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
    WORKING_DIRECTORY "${SOURCE_DIR}"
    OUTPUT_VARIABLE APPID_GIT_HASH_FULL
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
  )
  execute_process(
    COMMAND ${GIT_EXECUTABLE} describe --always --dirty
    WORKING_DIRECTORY "${SOURCE_DIR}"
    OUTPUT_VARIABLE APPID_GIT_DESCRIBE
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
  )
  if(APPID_GIT_DESCRIBE)
    set(APPID_GIT_HASH "${APPID_GIT_DESCRIBE}")
  endif()
endif()

if(NOT APPID_GIT_HASH)
  set(APPID_GIT_HASH "unknown")
endif()
if(NOT APPID_GIT_HASH_FULL)
  set(APPID_GIT_HASH_FULL "unknown")
endif()

set(HEADER_CONTENT
"// Generated from cmake/AppIdentity.cmake + git. Do not edit by hand.
#pragma once

namespace N64 {
namespace Ui {

inline constexpr const char *kAppDisplayName = \"${APPID_DISPLAY_NAME}\";
inline constexpr const char *kAppSlug = \"${APPID_SLUG}\";
inline constexpr const char *kAppGithubUrl = \"${APPID_GITHUB_URL}\";
inline constexpr const char *kAppGithubDisplay = \"${APPID_GITHUB_DISPLAY}\";
inline constexpr const char *kAppCopyright = \"${APPID_COPYRIGHT_LINE}\";
inline constexpr const char *kSettingsFileName = \"${APPID_SETTINGS_FILE}\";
inline constexpr const char *kWindowTitle = \"${APPID_WINDOW_TITLE}\";
inline constexpr const char *kGameWindowTitle = \"${APPID_GAME_WINDOW_TITLE}\";
inline constexpr const char *kCoreWindowTitle = \"${APPID_CORE_WINDOW_TITLE}\";
inline constexpr const char *kMenuOpenFolder = \"${APPID_MENU_OPEN_FOLDER}\";
inline constexpr const char *kMenuAbout = \"${APPID_MENU_ABOUT}\";
inline constexpr const char *kGitHash = \"${APPID_GIT_HASH}\";
inline constexpr const char *kGitHashFull = \"${APPID_GIT_HASH_FULL}\";

} // namespace Ui
} // namespace N64
")

file(MAKE_DIRECTORY "${OUTPUT_DIR}")
set(HEADER_TMP "${OUTPUT_DIR}/app_identity.h.tmp")
file(WRITE "${HEADER_TMP}" "${HEADER_CONTENT}")
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different
  "${HEADER_TMP}" "${OUTPUT_DIR}/app_identity.h")
file(REMOVE "${HEADER_TMP}")
