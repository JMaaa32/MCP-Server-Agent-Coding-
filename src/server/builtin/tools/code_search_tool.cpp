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

// 符号类型
struct CodeSymbol {
    std::string name;
    std::string type;       // "function", "class", "method", "variable"
    std::string file;       // 相对路径
    int line;
    std::string signature;  // 完整签名行
};

// 从文件中提取代码符号（轻量级 regex 方案，不依赖 Tree-sitter）
static std::vector<CodeSymbol> extract_symbols(const fs::path& file_path, const fs::path& sandbox_root) {
    std::vector<CodeSymbol> symbols;
    std::string rel_path = fs::relative(file_path, sandbox_root).string();

    std::ifstream ifs(file_path);
    if (!ifs.is_open()) return symbols;

    std::string ext = file_path.extension().string();
    std::string line;
    int line_num = 0;

    // Python 函数/类定义
    std::regex py_func(R"(^\s*(async\s+)?def\s+(\w+)\s*\()");
    std::regex py_class(R"(^\s*class\s+(\w+)\s*[\(:]?)");
    // C/C++ 函数定义（简化：返回类型 + 函数名 + 左括号）
    std::regex cpp_func(R"(^[\w\s\*&:<>]+\s+(\w+)\s*\([^;]*$)");
    std::regex cpp_class(R"(^\s*(class|struct)\s+(\w+))");
    // JavaScript/TypeScript
    std::regex js_func(R"(^\s*(export\s+)?(async\s+)?function\s+(\w+)\s*\()");
    std::regex js_class(R"(^\s*(export\s+)?class\s+(\w+))");
    std::regex js_arrow(R"(^\s*(export\s+)?(const|let|var)\s+(\w+)\s*=\s*(async\s+)?\([^)]*\)\s*=>)");

    while (std::getline(ifs, line)) {
        line_num++;
        std::smatch m;

        if (ext == ".py") {
            if (std::regex_search(line, m, py_func)) {
                symbols.push_back({m[2].str(), "function", rel_path, line_num, line});
            } else if (std::regex_search(line, m, py_class)) {
                symbols.push_back({m[1].str(), "class", rel_path, line_num, line});
            }
        } else if (ext == ".cpp" || ext == ".h" || ext == ".hpp" || ext == ".c" || ext == ".cc") {
            if (std::regex_search(line, m, cpp_class)) {
                symbols.push_back({m[2].str(), m[1].str(), rel_path, line_num, line});
            } else if (std::regex_search(line, m, cpp_func)) {
                // 过滤掉 if/for/while/switch 等关键字
                std::string fname = m[1].str();
                if (fname != "if" && fname != "for" && fname != "while" &&
                    fname != "switch" && fname != "return" && fname != "catch") {
                    symbols.push_back({fname, "function", rel_path, line_num, line});
                }
            }
        } else if (ext == ".js" || ext == ".ts" || ext == ".jsx" || ext == ".tsx") {
            if (std::regex_search(line, m, js_class)) {
                symbols.push_back({m[2].str(), "class", rel_path, line_num, line});
            } else if (std::regex_search(line, m, js_func)) {
                symbols.push_back({m[3].str(), "function", rel_path, line_num, line});
            } else if (std::regex_search(line, m, js_arrow)) {
                symbols.push_back({m[3].str(), "function", rel_path, line_num, line});
            }
        }
    }

    return symbols;
}

// 读取文件指定行附近的上下文
static std::string get_context(const fs::path& file_path, int target_line, int context_lines = 3) {
    std::ifstream ifs(file_path);
    if (!ifs.is_open()) return "";

    std::string line;
    int line_num = 0;
    std::ostringstream ctx;
    int start = std::max(1, target_line - context_lines);
    int end = target_line + context_lines;

    while (std::getline(ifs, line)) {
        line_num++;
        if (line_num >= start && line_num <= end) {
            ctx << line_num << ": " << line << "\n";
        }
        if (line_num > end) break;
    }

    return ctx.str();
}

// glob 转正则
static std::string glob_to_regex_cs(const std::string& glob) {
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

void register_code_search_tool(McpServer& mcp) {
    Tool tool;
    tool.name = "code_search";
    tool.description = "Search code in the workspace. Two modes: "
                       "(1) 'keyword' - search for text patterns in file contents, returns matching lines with context; "
                       "(2) 'symbol' - search for function/class/method definitions by name. "
                       "Supports file pattern filtering (e.g., *.py, *.cpp).";
    tool.input_schema.properties = {
        {"query", {{"type", "string"}, {"description", "Search query string"}}},
        {"mode", {{"type", "string"}, {"description", "Search mode: keyword or symbol (default: keyword)"},
                  {"enum", json::array({"keyword", "symbol"})}}},
        {"file_pattern", {{"type", "string"}, {"description", "Glob pattern to filter files (default: *)"}}},
        {"top_k", {{"type", "integer"}, {"description", "Maximum number of results (default: 10)"}}}
    };
    tool.input_schema.required = {"query"};

    mcp.register_tool(tool, [](const json& args) -> ToolResult {
        const std::string query = args.at("query").get<std::string>();
        std::string mode = args.value("mode", "keyword");
        std::string file_pattern = args.value("file_pattern", "*");
        int top_k = args.value("top_k", 10);

        // 沙箱
        fs::path sandbox_root;
        {
            const std::string cfg = MCP_CONFIG.GetWorkspaceRoot();
            sandbox_root = cfg.empty() ? fs::current_path() / "workspace" : fs::path(cfg);
            std::error_code mk_ec;
            fs::create_directories(sandbox_root, mk_ec);
            sandbox_root = fs::weakly_canonical(sandbox_root);
        }

        // 文件过滤正则
        std::regex file_regex;
        try {
            file_regex = std::regex(glob_to_regex_cs(file_pattern), std::regex::icase);
        } catch (const std::regex_error&) {
            return make_text_result("Error: invalid file pattern: " + file_pattern, true);
        }

        // 收集匹配的源文件
        std::vector<fs::path> source_files;
        for (const auto& entry : fs::recursive_directory_iterator(sandbox_root)) {
            if (!entry.is_regular_file()) continue;
            std::string filename = entry.path().filename().string();
            if (std::regex_match(filename, file_regex)) {
                source_files.push_back(entry.path());
            }
        }

        json results = json::array();

        if (mode == "symbol") {
            // 符号搜索：提取所有符号，匹配 query
            std::regex query_re(query, std::regex::icase);

            for (const auto& file : source_files) {
                auto symbols = extract_symbols(file, sandbox_root);
                for (const auto& sym : symbols) {
                    if (std::regex_search(sym.name, query_re)) {
                        std::string trimmed_sig = sym.signature;
                        // trim leading whitespace
                        size_t start = trimmed_sig.find_first_not_of(" \t");
                        if (start != std::string::npos) trimmed_sig = trimmed_sig.substr(start);

                        json item = {
                            {"file", sym.file},
                            {"line", sym.line},
                            {"type", sym.type},
                            {"name", sym.name},
                            {"snippet", trimmed_sig},
                            {"context", get_context(file, sym.line)}
                        };
                        results.push_back(item);

                        if (static_cast<int>(results.size()) >= top_k) break;
                    }
                }
                if (static_cast<int>(results.size()) >= top_k) break;
            }
        } else {
            // 关键词搜索：在文件内容中搜索匹配行
            std::regex query_re;
            try {
                query_re = std::regex(query, std::regex::icase);
            } catch (const std::regex_error&) {
                // 如果 query 不是有效正则，按字面量搜索
                std::string escaped;
                for (char c : query) {
                    if (std::string("\\^$.|?*+()[]{}").find(c) != std::string::npos) {
                        escaped += '\\';
                    }
                    escaped += c;
                }
                query_re = std::regex(escaped, std::regex::icase);
            }

            for (const auto& file : source_files) {
                std::ifstream ifs(file);
                if (!ifs.is_open()) continue;

                std::string line;
                int line_num = 0;
                std::string rel_path = fs::relative(file, sandbox_root).string();

                while (std::getline(ifs, line)) {
                    line_num++;
                    if (std::regex_search(line, query_re)) {
                        json item = {
                            {"file", rel_path},
                            {"line", line_num},
                            {"snippet", line},
                            {"context", get_context(file, line_num)}
                        };
                        results.push_back(item);

                        if (static_cast<int>(results.size()) >= top_k) break;
                    }
                }
                if (static_cast<int>(results.size()) >= top_k) break;
            }
        }

        json output = {
            {"results", results},
            {"total_matches", results.size()},
            {"mode", mode},
            {"query", query},
            {"file_pattern", file_pattern}
        };

        MCP_LOG_INFO("code_search: query='{}' mode={} matches={}", query, mode, results.size());
        return make_text_result(output.dump(2));
    });
}

} // namespace mcp
