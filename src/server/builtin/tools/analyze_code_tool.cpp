#include "tool_registrars.h"
#include "tool_result_utils.h"
#include "logger.h"
#include "config.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <regex>
#include <algorithm>
#include <vector>
#include <string>

namespace fs = std::filesystem;

namespace mcp {

// 代码复杂度分析结果
struct CodeMetrics {
    int total_lines = 0;
    int code_lines = 0;       // 非空非注释行
    int comment_lines = 0;
    int blank_lines = 0;
    int function_count = 0;
    int class_count = 0;
    int max_nesting = 0;      // 最大嵌套深度
    int cyclomatic_complexity = 0;  // 圈复杂度估算
    std::vector<std::string> imports;    // 导入/依赖
    std::vector<std::string> functions;  // 函数列表
    std::vector<std::string> classes;    // 类列表
};

// 分析 Python 文件
static CodeMetrics analyze_python(const std::string& content) {
    CodeMetrics m;
    std::istringstream iss(content);
    std::string line;
    bool in_multiline_comment = false;
    int current_indent = 0;
    int max_indent = 0;

    std::regex func_re(R"(^\s*(?:async\s+)?def\s+(\w+)\s*\()");
    std::regex class_re(R"(^\s*class\s+(\w+)\s*[\(:]?)");
    std::regex import_re(R"(^\s*(?:import|from)\s+(\S+))");
    // 圈复杂度: if/elif/for/while/except/and/or/with
    std::regex branch_re(R"(\b(if|elif|for|while|except|and|or)\b)");

    while (std::getline(iss, line)) {
        m.total_lines++;

        // 处理多行字符串注释
        if (in_multiline_comment) {
            m.comment_lines++;
            if (line.find("\"\"\"") != std::string::npos || line.find("'''") != std::string::npos) {
                in_multiline_comment = false;
            }
            continue;
        }

        // 去除首尾空白
        std::string trimmed = line;
        size_t start = trimmed.find_first_not_of(" \t");
        if (start == std::string::npos) {
            m.blank_lines++;
            continue;
        }
        trimmed = trimmed.substr(start);

        // 多行注释开始
        if (trimmed.substr(0, 3) == "\"\"\"" || trimmed.substr(0, 3) == "'''") {
            m.comment_lines++;
            // 检查是否同行关闭
            std::string quote = trimmed.substr(0, 3);
            if (trimmed.size() > 3 && trimmed.find(quote, 3) != std::string::npos) {
                continue;  // 同行关闭
            }
            in_multiline_comment = true;
            continue;
        }

        // 单行注释
        if (trimmed[0] == '#') {
            m.comment_lines++;
            continue;
        }

        m.code_lines++;

        // 缩进深度（用空格数 / 4 估算）
        current_indent = static_cast<int>(start) / 4;
        max_indent = std::max(max_indent, current_indent);

        // 匹配
        std::smatch match;
        if (std::regex_search(line, match, func_re)) {
            m.function_count++;
            m.functions.push_back(match[1].str());
        }
        if (std::regex_search(line, match, class_re)) {
            m.class_count++;
            m.classes.push_back(match[1].str());
        }
        if (std::regex_search(line, match, import_re)) {
            m.imports.push_back(match[1].str());
        }

        // 圈复杂度
        auto begin = std::sregex_iterator(line.begin(), line.end(), branch_re);
        auto end = std::sregex_iterator();
        m.cyclomatic_complexity += static_cast<int>(std::distance(begin, end));
    }

    m.max_nesting = max_indent;
    m.cyclomatic_complexity += 1;  // 基础复杂度
    return m;
}

// 分析 C/C++ 文件
static CodeMetrics analyze_cpp(const std::string& content) {
    CodeMetrics m;
    std::istringstream iss(content);
    std::string line;
    bool in_block_comment = false;
    int brace_depth = 0;
    int max_depth = 0;

    std::regex func_re(R"([\w\s\*&:<>]+\s+(\w+)\s*\([^;]*$)");
    std::regex class_re(R"(^\s*(class|struct)\s+(\w+))");
    std::regex include_re(R"(^\s*#include\s*[<"]([^>"]+)[>"])");
    std::regex branch_re(R"(\b(if|else if|for|while|switch|case|catch)\b)");

    while (std::getline(iss, line)) {
        m.total_lines++;

        if (in_block_comment) {
            m.comment_lines++;
            if (line.find("*/") != std::string::npos) {
                in_block_comment = false;
            }
            continue;
        }

        std::string trimmed = line;
        size_t start = trimmed.find_first_not_of(" \t");
        if (start == std::string::npos) {
            m.blank_lines++;
            continue;
        }
        trimmed = trimmed.substr(start);

        if (trimmed.substr(0, 2) == "/*") {
            m.comment_lines++;
            if (trimmed.find("*/") == std::string::npos) {
                in_block_comment = true;
            }
            continue;
        }

        if (trimmed.substr(0, 2) == "//") {
            m.comment_lines++;
            continue;
        }

        m.code_lines++;

        // 大括号深度
        for (char c : line) {
            if (c == '{') { brace_depth++; max_depth = std::max(max_depth, brace_depth); }
            if (c == '}') { brace_depth = std::max(0, brace_depth - 1); }
        }

        std::smatch match;
        if (std::regex_search(line, match, class_re)) {
            m.class_count++;
            m.classes.push_back(match[2].str());
        } else if (std::regex_search(line, match, func_re)) {
            std::string fname = match[1].str();
            if (fname != "if" && fname != "for" && fname != "while" && fname != "switch" && fname != "return") {
                m.function_count++;
                m.functions.push_back(fname);
            }
        }

        if (std::regex_search(line, match, include_re)) {
            m.imports.push_back(match[1].str());
        }

        auto begin = std::sregex_iterator(line.begin(), line.end(), branch_re);
        auto end = std::sregex_iterator();
        m.cyclomatic_complexity += static_cast<int>(std::distance(begin, end));
    }

    m.max_nesting = max_depth;
    m.cyclomatic_complexity += 1;
    return m;
}

void register_analyze_code_tool(McpServer& mcp) {
    Tool tool;
    tool.name = "analyze_code";
    tool.description = "Analyze code complexity and structure. Returns metrics: lines of code, "
                       "cyclomatic complexity, max nesting depth, function/class count, and imports/dependencies. "
                       "Supports Python (.py) and C/C++ (.cpp/.h/.c) files.";
    tool.input_schema.properties = {
        {"path", {{"type", "string"}, {"description", "File path relative to workspace/"}}},
    };
    tool.input_schema.required = {"path"};

    mcp.register_tool(tool, [](const json& args) -> ToolResult {
        const std::string raw_path = args.at("path").get<std::string>();

        // 沙箱验证
        fs::path sandbox_root;
        {
            const std::string cfg = MCP_CONFIG.GetWorkspaceRoot();
            sandbox_root = cfg.empty() ? fs::current_path() / "workspace" : fs::path(cfg);
            std::error_code mk_ec;
            fs::create_directories(sandbox_root, mk_ec);
            sandbox_root = fs::weakly_canonical(sandbox_root);
        }

        fs::path target = raw_path;
        if (target.is_relative()) {
            target = sandbox_root / target;
        }

        std::error_code ec;
        fs::path file_path = fs::weakly_canonical(target, ec);
        if (ec || file_path.string().substr(0, sandbox_root.string().size()) != sandbox_root.string()) {
            return make_text_result("Error: path is outside the workspace sandbox", true);
        }

        if (!fs::exists(file_path)) {
            return make_text_result("Error: file does not exist: " + raw_path, true);
        }

        // 读取文件
        std::ifstream ifs(file_path);
        if (!ifs.is_open()) {
            return make_text_result("Error: cannot open file: " + raw_path, true);
        }
        std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

        // 根据扩展名选择分析器
        std::string ext = file_path.extension().string();
        CodeMetrics metrics;
        std::string language;

        if (ext == ".py") {
            metrics = analyze_python(content);
            language = "python";
        } else if (ext == ".cpp" || ext == ".h" || ext == ".hpp" || ext == ".c" || ext == ".cc") {
            metrics = analyze_cpp(content);
            language = "c++";
        } else {
            return make_text_result("Error: unsupported file type: " + ext + " (supported: .py, .cpp, .h, .c)", true);
        }

        // 复杂度评级
        std::string complexity_rating;
        if (metrics.cyclomatic_complexity <= 5) complexity_rating = "low";
        else if (metrics.cyclomatic_complexity <= 15) complexity_rating = "moderate";
        else if (metrics.cyclomatic_complexity <= 30) complexity_rating = "high";
        else complexity_rating = "very_high";

        // 构造结果
        json result = {
            {"file", raw_path},
            {"language", language},
            {"metrics", {
                {"total_lines", metrics.total_lines},
                {"code_lines", metrics.code_lines},
                {"comment_lines", metrics.comment_lines},
                {"blank_lines", metrics.blank_lines},
                {"comment_ratio", metrics.code_lines > 0
                    ? static_cast<double>(metrics.comment_lines) / metrics.code_lines : 0.0},
                {"function_count", metrics.function_count},
                {"class_count", metrics.class_count},
                {"max_nesting_depth", metrics.max_nesting},
                {"cyclomatic_complexity", metrics.cyclomatic_complexity},
                {"complexity_rating", complexity_rating},
            }},
            {"functions", metrics.functions},
            {"classes", metrics.classes},
            {"imports", metrics.imports},
        };

        MCP_LOG_INFO("analyze_code: {} lang={} lines={} complexity={}({})",
                     raw_path, language, metrics.total_lines,
                     metrics.cyclomatic_complexity, complexity_rating);
        return make_text_result(result.dump(2));
    });
}

} // namespace mcp
