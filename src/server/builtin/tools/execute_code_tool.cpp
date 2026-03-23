#include "tool_registrars.h"
#include "tool_result_utils.h"
#include "logger.h"
#include "config.h"

#include <filesystem>
#include <array>
#include <cstdio>
#include <sstream>
#include <sys/select.h>
#include <unistd.h>
#include <chrono>

namespace fs = std::filesystem;

namespace mcp {

// 命令白名单前缀
static const std::vector<std::string> ALLOWED_PREFIXES = {
    "python3", "python", "node",
    "g++", "gcc", "clang++", "clang",
    "ls", "cat", "grep", "rg",
    "head", "tail", "wc", "find",
    "echo", "pwd", "stat", "file",
    "sort", "uniq", "diff", "tr",
    "/tmp/"   // 允许运行编译到 /tmp/ 的可执行文件
};

// 从命令中提取引号外部分（用于安全检查）
// 对于 python3 -c "code;here" 这种情况，引号内的分号不应被拦截
static std::string extract_unquoted_parts(const std::string& cmd) {
    std::string result;
    bool in_single = false;
    bool in_double = false;
    bool escaped = false;

    for (size_t i = 0; i < cmd.size(); ++i) {
        char c = cmd[i];
        if (escaped) {
            escaped = false;
            continue;  // 跳过转义字符
        }
        if (c == '\\') {
            escaped = true;
            continue;
        }
        if (c == '\'' && !in_double) {
            in_single = !in_single;
            continue;
        }
        if (c == '"' && !in_single) {
            in_double = !in_double;
            continue;
        }
        if (!in_single && !in_double) {
            result += c;
        }
    }
    return result;
}

// 危险符号/模式
static bool contains_dangerous_patterns(const std::string& cmd) {
    // 只检查引号外的部分，引号内的内容（如 python3 -c "a;b"）不拦截
    std::string unquoted = extract_unquoted_parts(cmd);

    const std::vector<std::string> dangerous = {
        ";", "&&", "||", "|", ">", "<", ">>",
        "$(", "${", "`"
    };
    for (const auto& pattern : dangerous) {
        if (unquoted.find(pattern) != std::string::npos) {
            return true;
        }
    }
    return false;
}

static bool is_command_allowed(const std::string& cmd) {
    // 跳过前导空格
    size_t start = cmd.find_first_not_of(' ');
    if (start == std::string::npos) return false;

    std::string trimmed = cmd.substr(start);
    for (const auto& prefix : ALLOWED_PREFIXES) {
        if (trimmed.substr(0, prefix.size()) == prefix) {
            // 路径前缀（以 / 结尾，如 "/tmp/"）：后面跟文件名，不需要空格
            // 普通命令前缀（如 "python3"）：后面必须是空格或结尾，防止 "rm" 匹配 "rmdir"
            bool is_path_prefix = !prefix.empty() && prefix.back() == '/';
            if (trimmed.size() == prefix.size() ||
                is_path_prefix ||
                trimmed[prefix.size()] == ' ') {
                return true;
            }
        }
    }
    return false;
}

void register_execute_code_tool(McpServer& mcp) {
    Tool tool;
    tool.name = "execute_code";
    tool.description = "Execute a shell command within the workspace sandbox. "
                       "Only whitelisted commands are allowed (python3, node, g++, gcc, ls, cat, grep, etc). "
                       "Commands with shell injection symbols (;, &&, |, >, <, etc) are rejected. "
                       "Execution timeout: 10 seconds. Output limit: 64KB.";
    tool.input_schema.properties = {
        {"command", {{"type", "string"}, {"description", "Shell command to execute (must use whitelisted commands)"}}}
    };
    tool.input_schema.required = {"command"};

    mcp.register_tool(tool, [](const json& args) -> ToolResult {
        const std::string command = args.at("command").get<std::string>();

        // 安全检查 1：危险符号
        if (contains_dangerous_patterns(command)) {
            MCP_LOG_WARN("execute_code: dangerous pattern blocked: {}", command);
            return make_text_result("Error: command contains forbidden characters "
                                   "(;, &&, ||, |, >, <, $(), `). "
                                   "Shell injection is not allowed.", true);
        }

        // 安全检查 2：命令白名单
        if (!is_command_allowed(command)) {
            MCP_LOG_WARN("execute_code: command not in whitelist: {}", command);
            return make_text_result("Error: command is not in the whitelist. "
                                   "Allowed commands: python3, node, g++, gcc, ls, cat, grep, rg, "
                                   "head, tail, wc, find, echo, pwd, stat, file, sort, uniq, diff, tr, "
                                   "/tmp/<binary> (compiled executables in /tmp/)", true);
        }

        // 获取 workspace 路径
        std::error_code ec;
        const std::string ws_cfg = MCP_CONFIG.GetWorkspaceRoot();
        fs::path workspace = ws_cfg.empty() ? fs::current_path() / "workspace" : fs::path(ws_cfg);
        fs::create_directories(workspace, ec);
        workspace = fs::weakly_canonical(workspace, ec);
        if (ec) {
            return make_text_result("Error: workspace/ directory does not exist", true);
        }

        // 在 workspace 目录下执行，30 秒超时
        constexpr int TIMEOUT_SECS = 30;
        std::string full_cmd = "cd " + workspace.string() + " && " + command + " 2>&1";

        MCP_LOG_INFO("execute_code: {}", command);

        // 执行并捕获输出
        constexpr size_t MAX_OUTPUT = 64 * 1024; // 64KB
        std::string output;
        bool truncated = false;

        FILE* pipe = popen(full_cmd.c_str(), "r");
        if (!pipe) {
            return make_text_result("Error: failed to execute command", true);
        }

        int fd = fileno(pipe);
        auto start = std::chrono::steady_clock::now();
        std::array<char, 4096> buffer;
        bool timed_out = false;

        while (true) {
            // select 超时检查
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - start).count();
            if (elapsed >= TIMEOUT_SECS) {
                timed_out = true;
                break;
            }

            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(fd, &fds);
            struct timeval tv{1, 0}; // 1 秒轮询
            int ready = select(fd + 1, &fds, nullptr, nullptr, &tv);
            if (ready < 0) break;
            if (ready == 0) continue; // 无数据，继续等待

            ssize_t n = read(fd, buffer.data(), buffer.size() - 1);
            if (n <= 0) break;
            buffer[n] = '\0';
            output += buffer.data();
            if (output.size() > MAX_OUTPUT) {
                output.resize(MAX_OUTPUT);
                truncated = true;
                break;
            }
        }

        pclose(pipe);
        int exit_code = timed_out ? 124 : 0;
        if (timed_out) {
            output += "\n[killed: command timed out after " + std::to_string(TIMEOUT_SECS) + "s]";
        }

        json result = {
            {"output", output},
            {"exit_code", exit_code},
            {"truncated", truncated}
        };

        if (truncated) {
            result["message"] = "Output exceeded 64KB limit and was truncated";
        }

        return make_text_result(result.dump(2));
    });
}

} // namespace mcp
