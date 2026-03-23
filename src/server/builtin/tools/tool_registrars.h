#pragma once

#include "mcp_server.h"

namespace mcp {

void register_echo_tool(McpServer& mcp);
void register_calculate_tool(McpServer& mcp);
void register_get_time_tool(McpServer& mcp);
void register_get_weather_tool(McpServer& mcp);
void register_write_file_tool(McpServer& mcp);
void register_analyze_text_file_tool(McpServer& mcp);

// Coding Agent 工具
void register_read_file_tool(McpServer& mcp);
void register_list_files_tool(McpServer& mcp);
void register_execute_code_tool(McpServer& mcp);
void register_run_tests_tool(McpServer& mcp);
void register_code_search_tool(McpServer& mcp);
void register_git_diff_tool(McpServer& mcp);
void register_analyze_code_tool(McpServer& mcp);

} // namespace mcp
