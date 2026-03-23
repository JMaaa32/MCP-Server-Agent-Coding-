// MCP 服务器核心类实现

#include "mcp_server.h"
#include <stdexcept>
#include "logger.h"

namespace mcp {

// 构造函数
McpServer::McpServer(const std::string& name, const std::string& version) {
    server_info_.name = name;
    server_info_.version = version;

    // 初始化服务器能力为空
    capabilities_.tools = ServerCapabilities::ToolsCapability{false};
    capabilities_.resources = ServerCapabilities::ResourcesCapability{false, false};
    capabilities_.prompts = ServerCapabilities::PromptsCapability{false};
}



// 获取初始化结果
InitializeResult McpServer::get_initialize_result() const {
    InitializeResult result;
    result.protocol_version = LATEST_PROTOCOL_VERSION;
    result.capabilities = capabilities_;
    result.server_info = server_info_;
    return result;
}
// 设置服务器能力
void McpServer::set_capabilities(const ServerCapabilities& capabilities) {
    capabilities_ = capabilities;
}



// 注册工具
void McpServer::register_tool(const Tool& tool, ToolHandler handler) {
    std::lock_guard<std::mutex> lock(tools_mutex_);

    if (tools_.find(tool.name) != tools_.end()) {
        throw std::runtime_error("Tool already registered: " + tool.name);
    }
    // MCP_LOG_INFO("Use tool name {}", tool.name);

    tools_[tool.name] = tool;
    tool_handlers_[tool.name] = handler;

    capabilities_.tools->list_changed = true;
}
// 列出所有工具
std::vector<Tool> McpServer::list_tools() const {
    std::lock_guard<std::mutex> lock(tools_mutex_);
    std::vector<Tool> tools;
    for (const auto& [name, tool] : tools_) {
        tools.push_back(tool);
    }
    return tools;
}
// 调用指定名称的工具，并处理事件推送
ToolResult McpServer::call_tool(const std::string& name, const json& arguments) {
    // 加锁确保多线程安全
    std::lock_guard<std::mutex> lock(tools_mutex_);
    
    // 查找工具处理器
    auto handler_iter = tool_handlers_.find(name);
    if (handler_iter == tool_handlers_.end()) {
        // 工具未注册
        throw std::runtime_error("Tool not found: " + name);
    }
    MCP_LOG_INFO("Use Tool: {}", name);

    // 发送工具调用开始的 SSE 事件
    {
        std::lock_guard<std::mutex> sse_lock(sse_mutex_);
        if (sse_callback_) {
            sse_callback_(json{
                {"type", "tool_call_start"},
                {"tool", name},
                {"arguments", arguments},
                {"timestamp", std::time(nullptr)}
            });
        }
    }

    try {
        // 调用工具处理器
        ToolResult tool_result = handler_iter->second(arguments);

        // 发送工具调用结束的 SSE 事件
        {
            std::lock_guard<std::mutex> sse_lock(sse_mutex_);
            if (sse_callback_) {
                sse_callback_(json{
                    {"type", "tool_call_end"},
                    {"tool", name},
                    {"arguments", arguments},
                    {"result", tool_result.to_json()},
                    {"timestamp", std::time(nullptr)}
                });
            }
        }
        return tool_result;
    } catch (const std::exception& e) {
        // 捕获异常，发送工具调用异常的 SSE 事件
        {
            std::lock_guard<std::mutex> sse_lock(sse_mutex_);
            if (sse_callback_) {
                sse_callback_(json{
                    {"type", "tool_call_error"},
                    {"tool", name},
                    {"error", e.what()},
                    {"timestamp", std::time(nullptr)}
                });
            }
        }

        // 封装错误结果返回
        ToolResult error_result;
        error_result.is_error = true;
        ContentItem error_item;
        error_item.type = "text";
        error_item.text = std::string("Error calling tool: ") + e.what();
        error_result.content.push_back(error_item);
        return error_result;
    }
}
// 检查工具是否存在
bool McpServer::has_tool(const std::string& name) const {
    std::lock_guard<std::mutex> lock(tools_mutex_);
    return tools_.find(name) != tools_.end();
}



// 注册资源
void McpServer::register_resource(const Resource& resource, ResourceProvider provider) {
    std::lock_guard<std::mutex> lock(resources_mutex_);
    if (resources_.find(resource.uri) != resources_.end()) {
        throw std::runtime_error("Resource already registered: " + resource.uri);
    }
    resources_[resource.uri] = resource;
    resource_providers_[resource.uri] = provider;
    capabilities_.resources->list_changed = true;
}
// 列出所有资源
std::vector<Resource> McpServer::list_resources() const {
    std::lock_guard<std::mutex> lock(resources_mutex_);
    std::vector<Resource> resources;
    for (const auto& [uri, resource] : resources_) {
        resources.push_back(resource);
    }
    return resources;
}
// 读取指定 URI 的资源内容
ResourceContent McpServer::read_resource(const std::string& uri) {
    std::lock_guard<std::mutex> lock(resources_mutex_);
    auto provider_iter = resource_providers_.find(uri);
    if (provider_iter == resource_providers_.end()) {
        throw std::runtime_error("Resource not found: " + uri);
    }
    return provider_iter->second(uri);
}
// 检查资源是否存在
bool McpServer::has_resource(const std::string& uri) const {
    std::lock_guard<std::mutex> lock(resources_mutex_);
    return resources_.find(uri) != resources_.end();
}


// 注册提示词
void McpServer::register_prompt(const Prompt& prompt, PromptGenerator generator) {
    std::lock_guard<std::mutex> lock(prompts_mutex_);
    if (prompts_.find(prompt.name) != prompts_.end()) {
        throw std::runtime_error("Prompt already registered: " + prompt.name);
    }
    prompts_[prompt.name] = prompt;
    prompt_generators_[prompt.name] = generator;
    capabilities_.prompts->list_changed = true;
}
// 列出所有提示词
std::vector<Prompt> McpServer::list_prompts() const {
    std::lock_guard<std::mutex> lock(prompts_mutex_);
    std::vector<Prompt> prompts;
    for (const auto& [name, prompt] : prompts_) {
        prompts.push_back(prompt);
    }
    return prompts;
}
// 获取指定名称的提示词，并处理事件推送
std::vector<PromptMessage> McpServer::get_prompt(const std::string& name, const json& arguments) {
    std::lock_guard<std::mutex> lock(prompts_mutex_);
    auto generator_iter = prompt_generators_.find(name);
    if (generator_iter == prompt_generators_.end()) {
        throw std::runtime_error("Prompt not found: " + name);
    }
    return generator_iter->second(arguments);
}
// 检查提示词是否存在
bool McpServer::has_prompt(const std::string& name) const {
    std::lock_guard<std::mutex> lock(prompts_mutex_);
    return prompts_.find(name) != prompts_.end();
}

// 设置 SSE 事件回调
void McpServer::set_sse_callback(SseEventCallback callback) {
    std::lock_guard<std::mutex> lock(sse_mutex_);
    sse_callback_ = callback;
}


} // namespace mcp
