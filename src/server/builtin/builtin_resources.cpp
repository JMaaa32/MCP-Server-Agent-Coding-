#include "builtin_resources.h"

#include "config.h"

#include <ctime>
#include <sstream>

namespace mcp {

void register_builtin_resources(McpServer& mcp) {
    {
        Resource res;
        res.uri = "system://info";
        res.name = "System Information";
        res.description = "Basic system information";
        res.mime_type = "text/plain";

        mcp.register_resource(res, [](const std::string& uri) -> ResourceContent {
            ResourceContent content;
            content.uri = uri;
            content.mime_type = "text/plain";

            std::ostringstream oss;
            oss << "MCP Server - System Info\n";
            oss << "========================\n";
            std::time_t now = std::time(nullptr);
            char buf[100];
            std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
            oss << "Time: " << buf << "\n";

            content.text = oss.str();
            return content;
        });
    }

    {
        Resource res;
        res.uri = "config://server";
        res.name = "Server Configuration";
        res.mime_type = "application/json";

        mcp.register_resource(res, [](const std::string& uri) -> ResourceContent {
            ResourceContent content;
            content.uri = uri;
            content.mime_type = "application/json";
            content.text = json({
                {"port", MCP_CONFIG.GetServerPort()},
                {"log_level", MCP_CONFIG.GetLogLevel()}
            }).dump(2);
            return content;
        });
    }
}

} // namespace mcp
