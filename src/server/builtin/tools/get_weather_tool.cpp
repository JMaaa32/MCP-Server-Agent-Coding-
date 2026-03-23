#include "tool_registrars.h"
#include "tool_result_utils.h"

#include <ctime>
#include <sstream>

namespace mcp {

void register_get_weather_tool(McpServer& mcp) {
    Tool tool;
    tool.name = "get_weather";
    tool.description = "Get weather information for a city";
    tool.input_schema.properties = {
        {"city", {{"type", "string"}, {"description", "City name (e.g., Beijing, Shanghai)"}}}
    };
    tool.input_schema.required = {"city"};

    mcp.register_tool(tool, [](const json& args) -> ToolResult {
        const std::string city = args.at("city").get<std::string>();

        std::time_t now = std::time(nullptr);
        char time_buf[100];
        std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));

        std::ostringstream weather_info;
        weather_info << "Weather Report for " << city << "\n";
        weather_info << "========================\n";
        weather_info << "Time: " << time_buf << "\n";
        weather_info << "Temperature: 22C\n";
        weather_info << "Condition: Sunny\n";
        weather_info << "Humidity: 45%\n";
        weather_info << "Wind: 5 km/h NE\n";
        weather_info << "\n(Note: This is simulated data. Integrate with real weather API for production)";
        return make_text_result(weather_info.str());
    });
}

} // namespace mcp
