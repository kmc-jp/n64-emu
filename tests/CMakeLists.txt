# TODO: テスト用実行ファイルの作成

# Dillonb's n64-tests
add_test(NAME basic COMMAND n64 --log-level debug --test "${CMAKE_SOURCE_DIR}/tests/dillonb-n64-tests/basic_simpleboot.z64")
