# MCP Library - 基础库（包含配置管理和日志系统）
add_library(mcp_lib STATIC
    config/config.cpp
    logger/logger.cpp
    json_rpc/stdio_jsonrpc.cpp
    json_rpc/jsonrpc_serialization.cpp
    json_rpc/http_jsonrpc.cpp
    mcp/types.cpp
    mcp/mcp_server.cpp
    server/builtin/builtin_registry.cpp
    server/builtin/builtin_tools.cpp
    server/builtin/builtin_resources.cpp
    server/builtin/builtin_prompts.cpp
    server/builtin/tools/echo_tool.cpp
    server/builtin/tools/calculate_tool.cpp
    server/builtin/tools/get_time_tool.cpp
    server/builtin/tools/get_weather_tool.cpp
    server/builtin/tools/write_file_tool.cpp
    server/builtin/tools/analyze_text_file_tool.cpp
    server/builtin/tools/read_file_tool.cpp
    server/builtin/tools/list_files_tool.cpp
    server/builtin/tools/execute_code_tool.cpp
    server/builtin/tools/run_tests_tool.cpp
    server/builtin/tools/code_search_tool.cpp
    server/builtin/tools/git_diff_tool.cpp
    server/builtin/tools/analyze_code_tool.cpp
)

# 设置 include 路径，这样就可以直接 #include "config.h" 而不需要写相对路径
target_include_directories(mcp_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/config
    ${CMAKE_CURRENT_SOURCE_DIR}/logger
    ${CMAKE_CURRENT_SOURCE_DIR}/json_rpc
    ${CMAKE_CURRENT_SOURCE_DIR}/mcp
    ${CMAKE_CURRENT_SOURCE_DIR}/server/builtin
)

target_link_libraries(mcp_lib
    Threads::Threads
)

# Link JSON library (using vcpkg)
target_link_libraries(mcp_lib nlohmann_json::nlohmann_json)

# Link spdlog library (using vcpkg)
target_link_libraries(mcp_lib spdlog::spdlog)

# Link cpp-httplib library (using vcpkg)
find_package(httplib CONFIG REQUIRED)
target_link_libraries(mcp_lib httplib::httplib)

# ===================================================================
# MCP Server - 统一服务器（支持 HTTP/stdio/both 三种模式）
# ===================================================================
add_executable(mcp_server
    mcp_server_main.cpp
)

target_link_libraries(mcp_server
    mcp_lib
)

# ===================================================================
# MCP Client SDK - 客户端库
# ===================================================================
add_library(mcp_client_sdk STATIC
    mcp_client/mcp_client.cpp
)

target_include_directories(mcp_client_sdk PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/mcp_client
    ${CMAKE_CURRENT_SOURCE_DIR}/mcp
)

target_link_libraries(mcp_client_sdk
    mcp_lib
    httplib::httplib
)

# ===================================================================
# 🔧 扩展位置：在这里添加更多库或可执行文件
# ===================================================================
