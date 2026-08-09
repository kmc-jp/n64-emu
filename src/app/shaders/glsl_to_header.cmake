# Convert GLSL shaders to C++ SPIR-V header arrays via glslc.
# Usage:
#   cmake -DGLSLC=... -DSHADER=... -DOUT=... -DVAR=... [-DSTAGE=...] -P glsl_to_header.cmake
# STAGE defaults from file extension: .comp/.vert/.frag

if(NOT GLSLC OR NOT SHADER OR NOT OUT OR NOT VAR)
  message(FATAL_ERROR "glsl_to_header.cmake requires GLSLC, SHADER, OUT, VAR")
endif()

if(NOT STAGE)
  get_filename_component(_ext "${SHADER}" EXT)
  if(_ext STREQUAL ".comp")
    set(STAGE compute)
  elseif(_ext STREQUAL ".vert")
    set(STAGE vertex)
  elseif(_ext STREQUAL ".frag")
    set(STAGE fragment)
  else()
    message(FATAL_ERROR "Cannot infer shader stage from ${SHADER}; pass -DSTAGE=")
  endif()
endif()

get_filename_component(_outdir "${OUT}" DIRECTORY)
file(MAKE_DIRECTORY "${_outdir}")

set(_spv "${OUT}.spv")
execute_process(
  COMMAND "${GLSLC}" -fshader-stage=${STAGE} -O -o "${_spv}" "${SHADER}"
  RESULT_VARIABLE _rc
  ERROR_VARIABLE _err
  OUTPUT_VARIABLE _out
)
if(NOT _rc EQUAL 0)
  message(FATAL_ERROR "glslc failed for ${SHADER}:\n${_err}${_out}")
endif()

file(READ "${_spv}" _bytes HEX)
string(LENGTH "${_bytes}" _hexlen)

set(_body "static const uint32_t ${VAR}[] = {\n")
set(_col 0)
set(_i 0)
while(_i LESS _hexlen)
  # File byte order b0 b1 b2 b3 -> little-endian word 0xB3B2B1B0
  string(SUBSTRING "${_bytes}" ${_i} 2 _b0)
  math(EXPR _i1 "${_i} + 2")
  string(SUBSTRING "${_bytes}" ${_i1} 2 _b1)
  math(EXPR _i2 "${_i} + 4")
  string(SUBSTRING "${_bytes}" ${_i2} 2 _b2)
  math(EXPR _i3 "${_i} + 6")
  string(SUBSTRING "${_bytes}" ${_i3} 2 _b3)
  set(_word "0x${_b3}${_b2}${_b1}${_b0}")
  if(_col EQUAL 0)
    string(APPEND _body "    ")
  endif()
  string(APPEND _body "${_word},")
  math(EXPR _col "${_col} + 1")
  if(_col EQUAL 6)
    string(APPEND _body "\n")
    set(_col 0)
  else()
    string(APPEND _body " ")
  endif()
  math(EXPR _i "${_i} + 8")
endwhile()
if(NOT _col EQUAL 0)
  string(APPEND _body "\n")
endif()
string(APPEND _body "};\n")

file(WRITE "${OUT}" "#pragma once\n#include <cstdint>\n\n${_body}")
file(REMOVE "${_spv}")
