#include "tool_registrars.h"
#include "tool_result_utils.h"
#include "logger.h"
#include "config.h"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace mcp {

// 沙箱根目录
static fs::path get_sandbox_root() {
    const std::string cfg = MCP_CONFIG.GetWorkspaceRoot();
    fs::path root = cfg.empty() ? fs::current_path() / "workspace" : fs::path(cfg);
    std::error_code ec;
    fs::create_directories(root, ec);
    return fs::weakly_canonical(root);
}

// 验证路径是否在沙箱内，返回规范化后的绝对路径
// 失败时返回空路径
static fs::path validate_sandbox_path(const std::string& raw_path, const fs::path& sandbox_root) {
    // 构造绝对路径：如果是相对路径，基于 sandbox_root 解析
    fs::path target = raw_path;
    if (target.is_relative()) {
        target = sandbox_root / target;
    }

    // 规范化（解析 .. 和 symlink）
    std::error_code ec;
    // 对于 read_file，文件必须存在才能 canonical
    // 对于不存在的路径，用 weakly_canonical
    fs::path canonical_path = fs::weakly_canonical(target, ec);
    if (ec) {
        return {};
    }

    // 检查是否在沙箱内
    auto sandbox_str = sandbox_root.string();
    auto target_str = canonical_path.string();
    if (target_str.substr(0, sandbox_str.size()) != sandbox_str) {
        return {};
    }

    return canonical_path;
}

void register_read_file_tool(McpServer& mcp) {
    Tool tool;
    tool.name = "read_file";
    tool.description = "Read the content of a file within the workspace sandbox. "
                       "Supports text files up to 1MB. Path must be relative to workspace/.";
    tool.input_schema.properties = {
        {"path", {{"type", "string"}, {"description", "File path relative to workspace/ directory"}}}
    };
    tool.input_schema.required = {"path"};

    mcp.register_tool(tool, [](const json& args) -> ToolResult {
        const std::string raw_path = args.at("path").get<std::string>();

        // 获取沙箱根目录
        fs::path sandbox_root;
        try {
            sandbox_root = get_sandbox_root();
        } catch (const std::exception&) {
            return make_text_result("Error: workspace/ directory does not exist", true);
        }

        // 沙箱验证
        fs::path file_path = validate_sandbox_path(raw_path, sandbox_root);
        if (file_path.empty()) {
            MCP_LOG_WARN("read_file: path traversal blocked: {}", raw_path);
            return make_text_result("Error: path is outside the workspace sandbox", true);
        }

        // 检查文件存在
        if (!fs::exists(file_path)) {
            return make_text_result("Error: file does not exist: " + raw_path, true);
        }

        if (!fs::is_regular_file(file_path)) {
            return make_text_result("Error: path is not a regular file: " + raw_path, true);
        }

        // 读取文件
        try {
            const auto file_size = fs::file_size(file_path);
            constexpr size_t MAX_SIZE = 1024 * 1024; // 1MB
            bool truncated = file_size > MAX_SIZE;

            std::ifstream ifs(file_path, std::ios::binary);
            if (!ifs.is_open()) {
                return make_text_result("Error: failed to open file: " + raw_path, true);
            }

            size_t read_size = truncated ? MAX_SIZE : static_cast<size_t>(file_size);
            std::string content(read_size, '\0');
            ifs.read(content.data(), static_cast<std::streamsize>(read_size));

            json result = {
                {"content", content},
                {"path", file_path.string()},
                {"size", file_size},
                {"truncated", truncated}
            };

            if (truncated) {
                result["message"] = "File exceeds 1MB limit, showing first 1MB only";
            }

            MCP_LOG_INFO("read_file: {} ({} bytes, truncated={})", raw_path, file_size, truncated);
            return make_text_result(result.dump(2));
        } catch (const std::exception& e) {
            return make_text_result(std::string("Error reading file: ") + e.what(), true);
        }
    });
}

} // namespace mcp
