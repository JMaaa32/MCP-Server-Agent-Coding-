#include "tool_registrars.h"
#include "tool_result_utils.h"
#include "logger.h"
#include "config.h"

#include <filesystem>
#include <array>
#include <cstdio>
#include <regex>

namespace fs = std::filesystem;

namespace mcp {

static fs::path get_sandbox_root_rt() {
    const std::string cfg = MCP_CONFIG.GetWorkspaceRoot();
    fs::path root = cfg.empty() ? fs::current_path() / "workspace" : fs::path(cfg);
    std::error_code ec;
    fs::create_directories(root, ec);
    return fs::weakly_canonical(root);
}

// 解析 pytest 输出，提取通过/失败数
static json parse_pytest_output(const std::string& output) {
    json result = {{"passed", 0}, {"failed", 0}, {"errors", 0}, {"total", 0}};

    // 匹配 "X passed", "X failed", "X error"
    std::regex passed_re(R"((\d+)\s+passed)");
    std::regex failed_re(R"((\d+)\s+failed)");
    std::regex error_re(R"((\d+)\s+error)");

    std::smatch m;
    if (std::regex_search(output, m, passed_re)) result["passed"] = std::stoi(m[1]);
    if (std::regex_search(output, m, failed_re)) result["failed"] = std::stoi(m[1]);
    if (std::regex_search(output, m, error_re))  result["errors"] = std::stoi(m[1]);

    result["total"] = result["passed"].get<int>() + result["failed"].get<int>() + result["errors"].get<int>();
    return result;
}

// 解析 C++ 测试输出（gtest 风格 或 简单 exit code）
static json parse_cpp_test_output(const std::string& output, int exit_code) {
    json result = {{"passed", 0}, {"failed", 0}, {"total", 0}};

    // 尝试匹配 gtest 格式 "[  PASSED  ] X tests"
    std::regex gtest_passed(R"(\[\s*PASSED\s*\]\s*(\d+)\s*test)");
    std::regex gtest_failed(R"(\[\s*FAILED\s*\]\s*(\d+)\s*test)");

    std::smatch m;
    if (std::regex_search(output, m, gtest_passed)) result["passed"] = std::stoi(m[1]);
    if (std::regex_search(output, m, gtest_failed)) result["failed"] = std::stoi(m[1]);

    result["total"] = result["passed"].get<int>() + result["failed"].get<int>();

    // 如果没有匹配到 gtest 格式，按 exit code 判断
    if (result["total"].get<int>() == 0) {
        result["total"] = 1;
        if (exit_code == 0) {
            result["passed"] = 1;
        } else {
            result["failed"] = 1;
        }
    }

    return result;
}

void register_run_tests_tool(McpServer& mcp) {
    Tool tool;
    tool.name = "run_tests";
    tool.description = "Compile and run tests for a given file within the workspace sandbox. "
                       "Supports Python (pytest) and C++ (compile + execute). "
                       "Returns test results with pass/fail counts.";
    tool.input_schema.properties = {
        {"test_file", {{"type", "string"}, {"description", "Test file path relative to workspace/"}}},
        {"language", {{"type", "string"}, {"description", "Programming language: python or cpp"},
                      {"enum", json::array({"python", "cpp"})}}}
    };
    tool.input_schema.required = {"test_file", "language"};

    mcp.register_tool(tool, [](const json& args) -> ToolResult {
        const std::string test_file = args.at("test_file").get<std::string>();
        const std::string language = args.at("language").get<std::string>();

        // 沙箱验证
        fs::path sandbox_root;
        try {
            sandbox_root = get_sandbox_root_rt();
        } catch (const std::exception&) {
            return make_text_result("Error: workspace/ directory does not exist", true);
        }

        fs::path file_path = sandbox_root / test_file;
        std::error_code ec;
        fs::path canonical_path = fs::weakly_canonical(file_path, ec);
        if (ec || canonical_path.string().substr(0, sandbox_root.string().size()) != sandbox_root.string()) {
            return make_text_result("Error: test file path is outside the workspace sandbox", true);
        }

        if (!fs::exists(canonical_path)) {
            return make_text_result("Error: test file does not exist: " + test_file, true);
        }

        // 构造执行命令
        std::string cmd;
        if (language == "python") {
            cmd = "cd " + sandbox_root.string() +
                  " && python3 -m pytest " + canonical_path.string() +
                  " -v --tb=short 2>&1";
        } else if (language == "cpp") {
            // 编译到临时文件，然后执行
            std::string bin_path = "/tmp/mcp_test_" + std::to_string(std::time(nullptr));
            cmd = "cd " + sandbox_root.string() +
                  " && g++ -std=c++17 " + canonical_path.string() +
                  " -o " + bin_path +
                  " 2>&1 && " + bin_path + " 2>&1";
        } else {
            return make_text_result("Error: unsupported language: " + language +
                                   ". Supported: python, cpp", true);
        }

        MCP_LOG_INFO("run_tests: {} ({})", test_file, language);

        // 执行
        constexpr size_t MAX_OUTPUT = 64 * 1024;
        std::string output;
        bool truncated = false;

        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) {
            return make_text_result("Error: failed to execute test command", true);
        }

        std::array<char, 4096> buffer;
        while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
            output += buffer.data();
            if (output.size() > MAX_OUTPUT) {
                output.resize(MAX_OUTPUT);
                truncated = true;
                break;
            }
        }

        int status = pclose(pipe);
        int exit_code = WEXITSTATUS(status);

        // 解析测试结果
        json test_stats;
        if (language == "python") {
            test_stats = parse_pytest_output(output);
        } else {
            test_stats = parse_cpp_test_output(output, exit_code);
        }

        json result = {
            {"output", output},
            {"exit_code", exit_code},
            {"language", language},
            {"test_file", test_file},
            {"test_results", test_stats},
            {"all_passed", exit_code == 0},
            {"truncated", truncated}
        };

        return make_text_result(result.dump(2));
    });
}

} // namespace mcp
