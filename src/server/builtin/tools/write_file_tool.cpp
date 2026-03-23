#include "tool_registrars.h"
#include "tool_result_utils.h"
#include "logger.h"
#include "config.h"

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace mcp {

void register_write_file_tool(McpServer& mcp) {
    Tool tool;
    tool.name = "write_file";
    tool.description = "Write content to a file within the workspace sandbox. "
                       "Path must be relative to workspace/ directory.";
    tool.input_schema.properties = {
        {"path", {{"type", "string"}, {"description", "File path relative to workspace/ directory"}}},
        {"content", {{"type", "string"}, {"description", "Content to write to the file"}}}
    };
    tool.input_schema.required = {"path", "content"};

    mcp.register_tool(tool, [](const json& args) -> ToolResult {
        const std::string raw_path = args.at("path").get<std::string>();
        const std::string content = args.at("content").get<std::string>();

        // 沙箱验证
        const std::string workspace_cfg = MCP_CONFIG.GetWorkspaceRoot();
        fs::path sandbox_root = workspace_cfg.empty()
            ? fs::current_path() / "workspace"
            : fs::path(workspace_cfg);
        std::error_code mk_ec;
        fs::create_directories(sandbox_root, mk_ec);
        sandbox_root = fs::weakly_canonical(sandbox_root);

        fs::path target = raw_path;
        if (target.is_relative()) {
            target = sandbox_root / target;
        }

        std::error_code ec;
        fs::path canonical_path = fs::weakly_canonical(target, ec);
        if (ec) {
            return make_text_result("Error: invalid path: " + raw_path, true);
        }

        auto sandbox_str = sandbox_root.string();
        if (canonical_path.string().substr(0, sandbox_str.size()) != sandbox_str) {
            MCP_LOG_WARN("write_file: path traversal blocked: {}", raw_path);
            return make_text_result("Error: path is outside the workspace sandbox", true);
        }

        // 创建父目录（如果不存在）
        fs::path parent = canonical_path.parent_path();
        if (!fs::exists(parent)) {
            fs::create_directories(parent, ec);
            if (ec) {
                return make_text_result("Error: failed to create directory: " + parent.string(), true);
            }
        }

        try {
            std::ofstream file(canonical_path);
            if (!file.is_open()) {
                return make_text_result("Error: failed to open file: " + raw_path, true);
            }

            file << content;
            file.close();

            MCP_LOG_INFO("write_file: {} ({} bytes)", raw_path, content.size());
            return make_text_result("Successfully wrote to file: " + raw_path);
        } catch (const std::exception& e) {
            return make_text_result(std::string("Error writing file: ") + e.what(), true);
        }
    });
}

} // namespace mcp
