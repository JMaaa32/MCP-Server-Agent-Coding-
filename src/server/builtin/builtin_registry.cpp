#include "builtin_registry.h"

#include "builtin_prompts.h"
#include "builtin_resources.h"
#include "builtin_tools.h"

namespace mcp {

void register_builtin_components(McpServer& mcp) {
    register_builtin_tools(mcp);
    register_builtin_resources(mcp);
    register_builtin_prompts(mcp);
}

} // namespace mcp
