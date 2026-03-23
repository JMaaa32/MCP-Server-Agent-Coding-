#include "tool_registrars.h"
#include "tool_result_utils.h"
#include "logger.h"

#include <array>
#include <cstdio>

namespace mcp {

static std::string exec_git_command(const std::string& cmd) {
    std::string output;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "Error: failed to execute git command";

    std::array<char, 4096> buffer;
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        output += buffer.data();
        // 限制输出大小
        if (output.size() > 64 * 1024) {
            output.resize(64 * 1024);
            output += "\n... (output truncated at 64KB)";
            break;
        }
    }
    pclose(pipe);
    return output;
}

void register_git_diff_tool(McpServer& mcp) {
    Tool tool;
    tool.name = "git_diff";
    tool.description = "Run git operations on the repository. Supports: "
                       "(1) 'diff' - show file changes (staged + unstaged); "
                       "(2) 'log' - show recent commit history; "
                       "(3) 'blame' - show line-by-line authorship of a file; "
                       "(4) 'status' - show working tree status.";
    tool.input_schema.properties = {
        {"operation", {{"type", "string"}, {"description", "Git operation: diff, log, blame, or status"},
                       {"enum", json::array({"diff", "log", "blame", "status"})}}},
        {"path", {{"type", "string"}, {"description", "File path for diff/blame (optional)"}}},
        {"n", {{"type", "integer"}, {"description", "Number of log entries to show (default: 5)"}}}
    };
    tool.input_schema.required = {"operation"};

    mcp.register_tool(tool, [](const json& args) -> ToolResult {
        const std::string operation = args.at("operation").get<std::string>();
        std::string path = args.value("path", "");
        int n = args.value("n", 5);

        std::string cmd;
        if (operation == "diff") {
            cmd = "git diff";
            if (!path.empty()) cmd += " -- " + path;
            // 同时包含 staged changes
            std::string staged = exec_git_command("git diff --cached" +
                (path.empty() ? std::string("") : " -- " + path));
            std::string unstaged = exec_git_command(cmd + " 2>&1");

            json result = {
                {"operation", "diff"},
                {"staged", staged},
                {"unstaged", unstaged}
            };
            if (!path.empty()) result["path"] = path;

            MCP_LOG_INFO("git_diff: diff {}", path.empty() ? "(all)" : path);
            return make_text_result(result.dump(2));

        } else if (operation == "log") {
            cmd = "git log --oneline -n " + std::to_string(n) + " 2>&1";
            std::string output = exec_git_command(cmd);

            json result = {
                {"operation", "log"},
                {"output", output},
                {"count", n}
            };

            MCP_LOG_INFO("git_diff: log -n {}", n);
            return make_text_result(result.dump(2));

        } else if (operation == "blame") {
            if (path.empty()) {
                return make_text_result("Error: blame requires a file path", true);
            }
            cmd = "git blame " + path + " 2>&1";
            std::string output = exec_git_command(cmd);

            json result = {
                {"operation", "blame"},
                {"path", path},
                {"output", output}
            };

            MCP_LOG_INFO("git_diff: blame {}", path);
            return make_text_result(result.dump(2));

        } else if (operation == "status") {
            std::string output = exec_git_command("git status --short 2>&1");

            json result = {
                {"operation", "status"},
                {"output", output}
            };

            MCP_LOG_INFO("git_diff: status");
            return make_text_result(result.dump(2));

        } else {
            return make_text_result("Error: unknown operation: " + operation +
                                   ". Supported: diff, log, blame, status", true);
        }
    });
}

} // namespace mcp
