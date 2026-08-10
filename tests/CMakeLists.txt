# Smoke testing
# 時間かかる�Eで最適化が強いReleaseでは走らせるよぁE��する

# Dillonb's n64-tests (interpreter; --test implies --headless)
add_test(NAME basic COMMAND kamo64-core --log-level=off --test "${CMAKE_SOURCE_DIR}/roms/dillonb-n64-tests/basic_simpleboot.z64")
add_test(NAME addiu COMMAND kamo64-core --log-level=off --test "${CMAKE_SOURCE_DIR}/roms/dillonb-n64-tests/addiu_simpleboot.z64")
add_test(NAME addu COMMAND kamo64-core --log-level=off --test "${CMAKE_SOURCE_DIR}/roms/dillonb-n64-tests/addu_simpleboot.z64")
add_test(NAME subu COMMAND kamo64-core --log-level=off --test "${CMAKE_SOURCE_DIR}/roms/dillonb-n64-tests/subu_simpleboot.z64")
add_test(NAME and COMMAND kamo64-core --log-level=off --test "${CMAKE_SOURCE_DIR}/roms/dillonb-n64-tests/and_simpleboot.z64")
add_test(NAME andi COMMAND kamo64-core --log-level=off --test "${CMAKE_SOURCE_DIR}/roms/dillonb-n64-tests/andi_simpleboot.z64")
add_test(NAME or COMMAND kamo64-core --log-level=off --test "${CMAKE_SOURCE_DIR}/roms/dillonb-n64-tests/or_simpleboot.z64")
add_test(NAME ori COMMAND kamo64-core --log-level=off --test "${CMAKE_SOURCE_DIR}/roms/dillonb-n64-tests/ori_simpleboot.z64")
add_test(NAME xor COMMAND kamo64-core --log-level=off --test "${CMAKE_SOURCE_DIR}/roms/dillonb-n64-tests/xor_simpleboot.z64")
add_test(NAME xori COMMAND kamo64-core --log-level=off --test "${CMAKE_SOURCE_DIR}/roms/dillonb-n64-tests/xor_simpleboot.z64")
add_test(NAME sll COMMAND kamo64-core --log-level=off --test "${CMAKE_SOURCE_DIR}/roms/dillonb-n64-tests/sll_simpleboot.z64")
add_test(NAME sllv COMMAND kamo64-core --log-level=off --test "${CMAKE_SOURCE_DIR}/roms/dillonb-n64-tests/sllv_simpleboot.z64")
add_test(NAME srlv COMMAND kamo64-core --log-level=off --test "${CMAKE_SOURCE_DIR}/roms/dillonb-n64-tests/srlv_simpleboot.z64")

# SD is unimplemented
# add_test(NAME slt COMMAND kamo64-core --log-level=off --test "${CMAKE_SOURCE_DIR}/roms/dillonb-n64-tests/slt_simpleboot.z64")
add_test(NAME sltu COMMAND kamo64-core --log-level=off --test "${CMAKE_SOURCE_DIR}/roms/dillonb-n64-tests/sltu_simpleboot.z64")

# Same suite with CPU dynarec
add_test(NAME jit_basic COMMAND kamo64-core --log-level=off --test --jit "${CMAKE_SOURCE_DIR}/roms/dillonb-n64-tests/basic_simpleboot.z64")
add_test(NAME jit_addiu COMMAND kamo64-core --log-level=off --test --jit "${CMAKE_SOURCE_DIR}/roms/dillonb-n64-tests/addiu_simpleboot.z64")
add_test(NAME jit_addu COMMAND kamo64-core --log-level=off --test --jit "${CMAKE_SOURCE_DIR}/roms/dillonb-n64-tests/addu_simpleboot.z64")
add_test(NAME jit_subu COMMAND kamo64-core --log-level=off --test --jit "${CMAKE_SOURCE_DIR}/roms/dillonb-n64-tests/subu_simpleboot.z64")
add_test(NAME jit_and COMMAND kamo64-core --log-level=off --test --jit "${CMAKE_SOURCE_DIR}/roms/dillonb-n64-tests/and_simpleboot.z64")
add_test(NAME jit_andi COMMAND kamo64-core --log-level=off --test --jit "${CMAKE_SOURCE_DIR}/roms/dillonb-n64-tests/andi_simpleboot.z64")
add_test(NAME jit_or COMMAND kamo64-core --log-level=off --test --jit "${CMAKE_SOURCE_DIR}/roms/dillonb-n64-tests/or_simpleboot.z64")
add_test(NAME jit_ori COMMAND kamo64-core --log-level=off --test --jit "${CMAKE_SOURCE_DIR}/roms/dillonb-n64-tests/ori_simpleboot.z64")
add_test(NAME jit_xor COMMAND kamo64-core --log-level=off --test --jit "${CMAKE_SOURCE_DIR}/roms/dillonb-n64-tests/xor_simpleboot.z64")
add_test(NAME jit_sll COMMAND kamo64-core --log-level=off --test --jit "${CMAKE_SOURCE_DIR}/roms/dillonb-n64-tests/sll_simpleboot.z64")
add_test(NAME jit_sllv COMMAND kamo64-core --log-level=off --test --jit "${CMAKE_SOURCE_DIR}/roms/dillonb-n64-tests/sllv_simpleboot.z64")
add_test(NAME jit_srlv COMMAND kamo64-core --log-level=off --test --jit "${CMAKE_SOURCE_DIR}/roms/dillonb-n64-tests/srlv_simpleboot.z64")
add_test(NAME jit_sltu COMMAND kamo64-core --log-level=off --test --jit "${CMAKE_SOURCE_DIR}/roms/dillonb-n64-tests/sltu_simpleboot.z64")

if(N64_RSP_SIMD)
    add_executable(rsp_vu_diff_test rsp_vu_diff_test.cpp)
    target_link_libraries(rsp_vu_diff_test PRIVATE rcp common log)
    add_test(NAME rsp_vu_diff COMMAND rsp_vu_diff_test)
endif()
