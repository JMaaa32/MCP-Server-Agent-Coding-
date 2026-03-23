#include "tool_registrars.h"
#include "tool_result_utils.h"

namespace mcp {

void register_echo_tool(McpServer& mcp) {
    Tool tool;
    tool.name = "echo";
    tool.description = "Echo back the input message";
    tool.input_schema.properties = {
        {"message", {{"type", "string"}, {"description", "Message to echo"}}}
    };
    tool.input_schema.required = {"message"};

    mcp.register_tool(tool, [](const json& args) -> ToolResult {
        return make_text_result("Echo: " + args.at("message").get<std::string>());
    });
}

} // namespace mcp
