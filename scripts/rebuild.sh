#!/bin/bash
set -e
BASE=/Users/mac/Desktop/CS/Cplusplus/mcp-tutorial
OUT=$BASE/build/src/CMakeFiles/mcp_lib.dir

CXX=/usr/bin/c++
FLAGS="-O3 -DNDEBUG -std=c++17 -arch arm64 -DCPPHTTPLIB_BROTLI_SUPPORT -DCPPHTTPLIB_USE_NON_BLOCKING_GETADDRINFO -DSPDLOG_COMPILED_LIB -DSPDLOG_FMT_EXTERNAL"
INC="-I$BASE/src/include -I$BASE/src/config -I$BASE/src/logger -I$BASE/src/json_rpc -I$BASE/src/mcp -I$BASE/src/server/builtin -isystem $BASE/build/vcpkg_installed/arm64-osx/include"

compile() {
    echo "Compiling $1..."
    $CXX $FLAGS $INC -c "$BASE/src/$1" -o "$OUT/$2"
}

compile config/config.cpp                           config/config.cpp.o
compile json_rpc/stdio_jsonrpc.cpp                  json_rpc/stdio_jsonrpc.cpp.o
compile logger/logger.cpp                           logger/logger.cpp.o
compile server/builtin/tools/write_file_tool.cpp    server/builtin/tools/write_file_tool.cpp.o
compile server/builtin/tools/read_file_tool.cpp     server/builtin/tools/read_file_tool.cpp.o
compile server/builtin/tools/list_files_tool.cpp    server/builtin/tools/list_files_tool.cpp.o
compile server/builtin/tools/run_tests_tool.cpp     server/builtin/tools/run_tests_tool.cpp.o
compile server/builtin/tools/code_search_tool.cpp   server/builtin/tools/code_search_tool.cpp.o
compile server/builtin/tools/execute_code_tool.cpp  server/builtin/tools/execute_code_tool.cpp.o
compile server/builtin/tools/analyze_code_tool.cpp  server/builtin/tools/analyze_code_tool.cpp.o

echo "Building archive..."
rm -f $BASE/build/src/libmcp_lib.a
/usr/bin/ar rcs $BASE/build/src/libmcp_lib.a \
    $OUT/config/config.cpp.o \
    $OUT/logger/logger.cpp.o \
    $OUT/json_rpc/stdio_jsonrpc.cpp.o \
    $OUT/json_rpc/jsonrpc_serialization.cpp.o \
    $OUT/json_rpc/http_jsonrpc.cpp.o \
    $OUT/mcp/types.cpp.o \
    $OUT/mcp/mcp_server.cpp.o \
    $OUT/server/builtin/builtin_registry.cpp.o \
    $OUT/server/builtin/builtin_tools.cpp.o \
    $OUT/server/builtin/builtin_resources.cpp.o \
    $OUT/server/builtin/builtin_prompts.cpp.o \
    $OUT/server/builtin/tools/echo_tool.cpp.o \
    $OUT/server/builtin/tools/calculate_tool.cpp.o \
    $OUT/server/builtin/tools/get_time_tool.cpp.o \
    $OUT/server/builtin/tools/get_weather_tool.cpp.o \
    $OUT/server/builtin/tools/write_file_tool.cpp.o \
    $OUT/server/builtin/tools/analyze_text_file_tool.cpp.o \
    $OUT/server/builtin/tools/read_file_tool.cpp.o \
    $OUT/server/builtin/tools/list_files_tool.cpp.o \
    $OUT/server/builtin/tools/execute_code_tool.cpp.o \
    $OUT/server/builtin/tools/run_tests_tool.cpp.o \
    $OUT/server/builtin/tools/code_search_tool.cpp.o \
    $OUT/server/builtin/tools/git_diff_tool.cpp.o \
    $OUT/server/builtin/tools/analyze_code_tool.cpp.o

echo "Linking..."
$CXX -O3 -DNDEBUG -arch arm64 -Wl,-search_paths_first -Wl,-headerpad_max_install_names \
    $BASE/build/src/CMakeFiles/mcp_server.dir/mcp_server_main.cpp.o \
    -o $BASE/build/src/mcp_server \
    $BASE/build/src/libmcp_lib.a \
    $BASE/build/vcpkg_installed/arm64-osx/lib/libspdlog.a \
    $BASE/build/vcpkg_installed/arm64-osx/lib/libfmt.a \
    -framework CFNetwork -framework CoreFoundation \
    $BASE/build/vcpkg_installed/arm64-osx/lib/libbrotlienc.a \
    $BASE/build/vcpkg_installed/arm64-osx/lib/libbrotlidec.a \
    $BASE/build/vcpkg_installed/arm64-osx/lib/libbrotlicommon.a \
    -lm

echo "Done: $BASE/build/src/mcp_server"
