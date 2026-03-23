#include "tool_registrars.h"
#include "tool_result_utils.h"

namespace mcp {

void register_calculate_tool(McpServer& mcp) {
    Tool tool;
    tool.name = "calculate";
    tool.description = "Perform basic arithmetic operations (add, subtract, multiply, divide).";
    tool.input_schema.properties = {
        {"operation", {
            {"type", "string"},
            {"description", "The operation to perform: add, subtract, multiply, or divide."},
            {"enum", json::array({"add", "subtract", "multiply", "divide"})}
        }},
        {"a", {
            {"type", "number"},
            {"description", "The first operand."}
        }},
        {"b", {
            {"type", "number"},
            {"description", "The second operand."}
        }}
    };
    tool.input_schema.required = {"operation", "a", "b"};

    mcp.register_tool(tool, [](const json& args) -> ToolResult {
        const std::string op = args.at("operation").get<std::string>();
        const double a = args.at("a").get<double>();
        const double b = args.at("b").get<double>();
        double result_val = 0.0;

        if (op == "add") {
            result_val = a + b;
        } else if (op == "subtract") {
            result_val = a - b;
        } else if (op == "multiply") {
            result_val = a * b;
        } else if (op == "divide") {
            if (b == 0) {
                return make_text_result("Error: Division by zero", true);
            }
            result_val = a / b;
        } else {
            return make_text_result("Error: Unsupported operation '" + op + "'", true);
        }

        return make_text_result("Result: " + std::to_string(result_val));
    });
}

} // namespace mcp
