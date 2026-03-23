#include "builtin_tools.h"

#include "tools/tool_registrars.h"

namespace mcp {

void register_builtin_tools(McpServer& mcp) {
    register_echo_tool(mcp);
    register_calculate_tool(mcp);
    register_get_time_tool(mcp);
    register_get_weather_tool(mcp);
    register_write_file_tool(mcp);
    register_analyze_text_file_tool(mcp);

    // Coding Agent 工具
    register_read_file_tool(mcp);
    register_list_files_tool(mcp);
    register_execute_code_tool(mcp);
    register_run_tests_tool(mcp);
    register_code_search_tool(mcp);
    register_git_diff_tool(mcp);
    register_analyze_code_tool(mcp);
}

} // namespace mcp
