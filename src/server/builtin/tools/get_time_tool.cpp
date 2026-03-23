#include "tool_registrars.h"
#include "tool_result_utils.h"

#include <ctime>

namespace mcp {

void register_get_time_tool(McpServer& mcp) {
    Tool tool;
    tool.name = "get_time";
    tool.description = "Get current system time";
    tool.input_schema.properties = json::object();

    mcp.register_tool(tool, [](const json&) -> ToolResult {
        std::time_t now = std::time(nullptr);
        char buf[100];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
        return make_text_result(buf);
    });
}

} // namespace mcp
