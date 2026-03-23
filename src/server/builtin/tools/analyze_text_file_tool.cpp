#include "tool_registrars.h"
#include "tool_result_utils.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <utility>
#include <vector>

namespace mcp {

namespace {

std::string normalize_word(const std::string& raw) {
    std::string word;
    word.reserve(raw.size());
    for (char ch : raw) {
        const unsigned char uch = static_cast<unsigned char>(ch);
        if (std::isalnum(uch)) {
            word.push_back(static_cast<char>(std::tolower(uch)));
        }
    }
    return word;
}

} // namespace

void register_analyze_text_file_tool(McpServer& mcp) {
    Tool tool;
    tool.name = "analyze_text_file";
    tool.description = "Analyze a text file with stats/search/top words for engineering diagnostics.";
    tool.input_schema.properties = {
        {"path", {{"type", "string"}, {"description", "Target text file path."}}},
        {"mode", {{"type", "string"}, {"enum", json::array({"stats", "search", "top_words"})},
                  {"description", "Analysis mode: stats, search, or top_words."}}},
        {"query", {{"type", "string"}, {"description", "Search keyword (required when mode=search)."}}},
        {"maxResults", {{"type", "number"}, {"description", "Maximum output rows; default 20."}}}
    };
    tool.input_schema.required = {"path"};

    mcp.register_tool(tool, [](const json& args) -> ToolResult {
        const std::string path = args.at("path").get<std::string>();
        const std::string mode = args.value("mode", "stats");
        const std::string query = args.value("query", "");
        int max_results = args.value("maxResults", 20);
        if (max_results <= 0) {
            max_results = 20;
        }
        if (max_results > 200) {
            max_results = 200;
        }

        std::ifstream file(path);
        if (!file.is_open()) {
            return make_text_result("Error: Failed to open file: " + path, true);
        }

        std::vector<std::string> lines;
        lines.reserve(1024);
        std::string line;
        size_t char_count = 0;
        size_t non_empty_lines = 0;

        while (std::getline(file, line)) {
            char_count += line.size() + 1;
            if (!line.empty()) {
                ++non_empty_lines;
            }
            lines.push_back(line);
        }

        if (mode == "stats") {
            size_t word_count = 0;
            size_t max_line_len = 0;
            std::vector<size_t> line_lengths;
            line_lengths.reserve(lines.size());

            for (const auto& current : lines) {
                std::istringstream iss(current);
                std::string token;
                while (iss >> token) {
                    ++word_count;
                }
                max_line_len = std::max(max_line_len, current.size());
                line_lengths.push_back(current.size());
            }

            std::sort(line_lengths.begin(), line_lengths.end());
            const size_t p95_line_len = line_lengths.empty()
                ? 0
                : line_lengths[static_cast<size_t>(line_lengths.size() * 0.95) < line_lengths.size()
                    ? static_cast<size_t>(line_lengths.size() * 0.95)
                    : line_lengths.size() - 1];

            std::ostringstream out;
            out << "Text Analysis (stats)\n";
            out << "=====================\n";
            out << "Path: " << path << "\n";
            out << "Lines: " << lines.size() << "\n";
            out << "Non-empty lines: " << non_empty_lines << "\n";
            out << "Words: " << word_count << "\n";
            out << "Characters (approx): " << char_count << "\n";
            out << "Max line length: " << max_line_len << "\n";
            out << "P95 line length: " << p95_line_len << "\n";
            out << "Estimated reading time: " << std::max<size_t>(1, word_count / 200) << " min\n";
            return make_text_result(out.str());
        }

        if (mode == "search") {
            if (query.empty()) {
                return make_text_result("Error: 'query' is required when mode is 'search'.", true);
            }

            std::vector<std::pair<size_t, std::string>> matches;
            matches.reserve(static_cast<size_t>(max_results));
            for (size_t i = 0; i < lines.size() && static_cast<int>(matches.size()) < max_results; ++i) {
                if (lines[i].find(query) != std::string::npos) {
                    matches.push_back({i + 1, lines[i]});
                }
            }

            std::ostringstream out;
            out << "Text Analysis (search)\n";
            out << "======================\n";
            out << "Path: " << path << "\n";
            out << "Query: " << query << "\n";
            out << "Matched lines (showing up to " << max_results << "): " << matches.size() << "\n\n";

            if (matches.empty()) {
                out << "No matches found.\n";
            } else {
                for (const auto& match : matches) {
                    out << "L" << match.first << ": " << match.second << "\n";
                }
            }
            return make_text_result(out.str());
        }

        if (mode == "top_words") {
            static const std::unordered_map<std::string, bool> stop_words = {
                {"the", true}, {"and", true}, {"for", true}, {"with", true}, {"this", true},
                {"that", true}, {"from", true}, {"have", true}, {"you", true}, {"are", true},
                {"not", true}, {"but", true}, {"was", true}, {"were", true}, {"has", true}
            };

            std::unordered_map<std::string, int> freq;
            for (const auto& current : lines) {
                std::istringstream iss(current);
                std::string token;
                while (iss >> token) {
                    std::string word = normalize_word(token);
                    if (word.size() < 3 || stop_words.count(word) > 0) {
                        continue;
                    }
                    ++freq[word];
                }
            }

            std::vector<std::pair<std::string, int>> ranked(freq.begin(), freq.end());
            std::sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) {
                if (a.second != b.second) {
                    return a.second > b.second;
                }
                return a.first < b.first;
            });

            std::ostringstream out;
            out << "Text Analysis (top_words)\n";
            out << "=========================\n";
            out << "Path: " << path << "\n";
            out << "Unique words: " << ranked.size() << "\n";
            out << "Top words (up to " << max_results << "):\n\n";

            const size_t limit = std::min(ranked.size(), static_cast<size_t>(max_results));
            for (size_t i = 0; i < limit; ++i) {
                out << (i + 1) << ". " << ranked[i].first << " => " << ranked[i].second << "\n";
            }

            if (limit == 0) {
                out << "No tokens found after normalization.\n";
            }
            return make_text_result(out.str());
        }

        return make_text_result("Error: Unsupported mode '" + mode + "'.", true);
    });
}

} // namespace mcp
