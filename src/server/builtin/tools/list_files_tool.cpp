#include "tool_registrars.h"
#include "tool_result_utils.h"
#include "logger.h"
#include "config.h"

#include <filesystem>
#include <regex>
#include <sys/stat.h>

namespace fs = std::filesystem;

namespace mcp {

static fs::path get_sandbox_root_lf() {
    const std::string cfg = MCP_CONFIG.GetWorkspaceRoot();
    fs::path root = cfg.empty() ? fs::current_path() / "workspace" : fs::path(cfg);
    std::error_code ec;
    fs::create_directories(root, ec);
    return fs::weakly_canonical(root);
}

static fs::path validate_sandbox_path_lf(const std::string& raw_path, const fs::path& sandbox_root) {
    fs::path target = raw_path;
    if (target.is_relative()) {
        target = sandbox_root / target;
    }
    std::error_code ec;
    fs::path canonical_path = fs::weakly_canonical(target, ec);
    if (ec) return {};

    auto sandbox_str = sandbox_root.string();
    auto target_str = canonical_path.string();
    if (target_str.substr(0, sandbox_str.size()) != sandbox_str) {
        return {};
    }
    return canonical_path;
}

// 简易 glob 转正则：*.cpp → .*\.cpp
static std::string glob_to_regex(const std::string& glob) {
    std::string regex_str;
    for (char c : glob) {
        switch (c) {
            case '*': regex_str += ".*"; break;
            case '?': regex_str += "."; break;
            case '.': regex_str += "\\."; break;
            default:  regex_str += c; break;
        }
    }
    return regex_str;
}

void register_list_files_tool(McpServer& mcp) {
    Tool tool;
    tool.name = "list_files";
    tool.description = "List files in a directory within the workspace sandbox. "
                       "Supports glob patterns (e.g., *.py, *.cpp) and recursive traversal.";
    tool.input_schema.properties = {
        {"directory", {{"type", "string"}, {"description", "Directory path relative to workspace/ (default: .)"}}},
        {"pattern", {{"type", "string"}, {"description", "Glob pattern to filter files (default: *)"}}},
        {"recursive", {{"type", "boolean"}, {"description", "Whether to search recursively (default: false)"}}}
    };
    tool.input_schema.required = {};

    mcp.register_tool(tool, [](const json& args) -> ToolResult {
        std::string dir_str = args.value("directory", ".");
        std::string pattern = args.value("pattern", "*");
        bool recursive = args.value("recursive", false);

        // 沙箱验证
        fs::path sandbox_root;
        try {
            sandbox_root = get_sandbox_root_lf();
        } catch (const std::exception&) {
            return make_text_result("Error: workspace/ directory does not exist", true);
        }

        fs::path dir_path = validate_sandbox_path_lf(dir_str, sandbox_root);
        if (dir_path.empty()) {
            return make_text_result("Error: directory path is outside the workspace sandbox", true);
        }

        if (!fs::exists(dir_path) || !fs::is_directory(dir_path)) {
            return make_text_result("Error: directory does not exist: " + dir_str, true);
        }

        // 构造正则匹配
        std::regex file_regex;
        try {
            file_regex = std::regex(glob_to_regex(pattern), std::regex::icase);
        } catch (const std::regex_error&) {
            return make_text_result("Error: invalid glob pattern: " + pattern, true);
        }

        json files_arr = json::array();
        constexpr int MAX_FILES = 1000;

        try {
            auto process_entry = [&](const fs::directory_entry& entry) {
                if (static_cast<int>(files_arr.size()) >= MAX_FILES) return;

                std::string filename = entry.path().filename().string();
                if (!std::regex_match(filename, file_regex) && !entry.is_directory()) {
                    return;
                }

                // 获取相对路径（相对于 sandbox_root）
                auto rel_path = fs::relative(entry.path(), sandbox_root);

                json file_info = {
                    {"path", rel_path.string()},
                    {"is_directory", entry.is_directory()}
                };

                std::error_code ec;
                if (!entry.is_directory()) {
                    auto fsize = entry.file_size(ec);
                    if (!ec) file_info["size"] = fsize;
                }

                // 获取最后修改时间（用 stat 兼容 macOS）
                struct stat st;
                if (::stat(entry.path().c_str(), &st) == 0) {
                    file_info["modified_time"] = static_cast<int64_t>(st.st_mtime);
                }

                files_arr.push_back(file_info);
            };

            if (recursive) {
                for (const auto& entry : fs::recursive_directory_iterator(dir_path)) {
                    process_entry(entry);
                }
            } else {
                for (const auto& entry : fs::directory_iterator(dir_path)) {
                    process_entry(entry);
                }
            }
        } catch (const std::exception& e) {
            return make_text_result(std::string("Error listing directory: ") + e.what(), true);
        }

        json result = {
            {"files", files_arr},
            {"count", files_arr.size()},
            {"directory", dir_str},
            {"pattern", pattern},
            {"recursive", recursive}
        };

        if (static_cast<int>(files_arr.size()) >= MAX_FILES) {
            result["truncated"] = true;
            result["message"] = "Result limited to 1000 entries";
        }

        MCP_LOG_INFO("list_files: {} ({} entries)", dir_str, files_arr.size());
        return make_text_result(result.dump(2));
    });
}

} // namespace mcp
