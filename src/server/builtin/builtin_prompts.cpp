#include "builtin_prompts.h"

namespace mcp {

void register_builtin_prompts(McpServer& mcp) {
    Prompt prompt;
    prompt.name = "code_review";
    prompt.description = "Generate code review prompt";
    prompt.arguments.push_back(PromptArgument{.name = "code", .required = true});
    prompt.arguments.push_back(PromptArgument{.name = "language", .required = true});

    mcp.register_prompt(prompt, [](const json& args) -> std::vector<PromptMessage> {
        std::vector<PromptMessage> msgs;
        PromptMessage msg;
        msg.role = Role::User;
        msg.content = {
            {"type", "text"},
            {"text", "Please review this " + args.at("language").get<std::string>() +
                     " code:\n\n" + args.at("code").get<std::string>()}
        };
        msgs.push_back(msg);
        return msgs;
    });
}

} // namespace mcp
