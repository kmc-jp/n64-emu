# Smoke testing
# 時間かかるので最適化が強いReleaseでは走らせるようにする

# Dillonb's n64-tests
add_test(NAME basic COMMAND n64 --log-level=debug --test "${CMAKE_SOURCE_DIR}/tests/dillonb-n64-tests/basic_simpleboot.z64")
add_test(NAME addiu COMMAND n64 --log-level=debug --test "${CMAKE_SOURCE_DIR}/tests/dillonb-n64-tests/addiu_simpleboot.z64")
add_test(NAME addu COMMAND n64 --log-level=debug --test "${CMAKE_SOURCE_DIR}/tests/dillonb-n64-tests/addu_simpleboot.z64")
add_test(NAME subu COMMAND n64 --log-level=debug --test "${CMAKE_SOURCE_DIR}/tests/dillonb-n64-tests/subu_simpleboot.z64")
add_test(NAME and COMMAND n64 --log-level=debug --test "${CMAKE_SOURCE_DIR}/tests/dillonb-n64-tests/and_simpleboot.z64")
add_test(NAME andi COMMAND n64 --log-level=debug --test "${CMAKE_SOURCE_DIR}/tests/dillonb-n64-tests/andi_simpleboot.z64")
add_test(NAME or COMMAND n64 --log-level=debug --test "${CMAKE_SOURCE_DIR}/tests/dillonb-n64-tests/or_simpleboot.z64")
add_test(NAME ori COMMAND n64 --log-level=debug --test "${CMAKE_SOURCE_DIR}/tests/dillonb-n64-tests/ori_simpleboot.z64")
add_test(NAME xor COMMAND n64 --log-level=debug --test "${CMAKE_SOURCE_DIR}/tests/dillonb-n64-tests/xor_simpleboot.z64")
add_test(NAME xori COMMAND n64 --log-level=debug --test "${CMAKE_SOURCE_DIR}/tests/dillonb-n64-tests/xor_simpleboot.z64")
add_test(NAME sll COMMAND n64 --log-level=debug --test "${CMAKE_SOURCE_DIR}/tests/dillonb-n64-tests/sll_simpleboot.z64")
add_test(NAME sllv COMMAND n64 --log-level=debug --test "${CMAKE_SOURCE_DIR}/tests/dillonb-n64-tests/sllv_simpleboot.z64")
add_test(NAME srlv COMMAND n64 --log-level=debug --test "${CMAKE_SOURCE_DIR}/tests/dillonb-n64-tests/srlv_simpleboot.z64")

# SD is unimplemented
# add_test(NAME slt COMMAND n64 --log-level=debug --test "${CMAKE_SOURCE_DIR}/tests/dillonb-n64-tests/slt_simpleboot.z64")
add_test(NAME sltu COMMAND n64 --log-level=debug --test "${CMAKE_SOURCE_DIR}/tests/dillonb-n64-tests/sltu_simpleboot.z64")
