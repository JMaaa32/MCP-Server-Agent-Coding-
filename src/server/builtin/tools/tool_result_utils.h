#pragma once

#include "mcp_server.h"

namespace mcp {

inline ToolResult make_text_result(const std::string& text, bool is_error = false) {
    ToolResult result;
    result.is_error = is_error;
    result.content.push_back(ContentItem{
        .type = "text",
        .text = text
    });
    return result;
}

} // namespace mcp
