/**
 * @file mcp_client.h
 * @brief MCP 客户端 SDK - 方便其他程序调用 MCP 服务器
 */

#pragma once

#include "types.h"
#include <string>
#include <memory>
#include <vector>

namespace mcp {

// 抽象传输层接口：
// 屏蔽具体通信细节（HTTP/stdio/...），上层只关心请求与响应 JSON。
class Transport {
public:
    virtual ~Transport() = default;
    // 发送一个 JSON-RPC 请求并返回响应 JSON。
    virtual json send(const json& request) = 0;
};

// HTTP 传输实现（基于 /jsonrpc 端点）。
class HttpTransport : public Transport {
public:
    // host: 服务器地址（如 "127.0.0.1"）
    // port: 服务器端口（如 8080）
    HttpTransport(const std::string& host, int port);
    ~HttpTransport();

    // 发送 JSON-RPC 请求，返回服务端 JSON 响应。
    json send(const json& request) override;

private:
    // Pimpl：隐藏第三方库与实现细节，降低头文件耦合。
    class Impl;
    std::unique_ptr<Impl> impl_;
};

// MCP 客户端：
// 对外提供 MCP 常用能力（tools/resources/prompts）的一组类型安全封装。
class McpClient {
public:
    // 通过 HTTP 连接 MCP 服务端。
    McpClient(const std::string& host, int port);

    ~McpClient();

    // 初始化握手：返回协议版本、服务端能力与 server info。
    InitializeResult initialize();

    // 获取工具列表（tools/list）。
    std::vector<Tool> list_tools();

    // 调用指定工具（tools/call）。
    // name: 工具名
    // arguments: 工具参数 JSON 对象
    ToolResult call_tool(const std::string& name, const json& arguments);

    // 获取资源列表（resources/list）。
    std::vector<Resource> list_resources();

    // 读取资源内容（resources/read）。
    // uri: 资源唯一标识
    ResourceContent read_resource(const std::string& uri);

    // 获取提示词模板列表（prompts/list）。
    std::vector<Prompt> list_prompts();

    // 获取并渲染指定提示词（prompts/get）。
    // name: 提示词模板名
    // arguments: 模板参数
    std::vector<PromptMessage> get_prompt(const std::string& name, const json& arguments);

    // 返回最近一次请求失败的错误信息；成功时通常为空字符串。
    std::string get_last_error() const;
    // 禁止拷贝：避免复制 transport 连接状态与 request id 状态。
    McpClient(const McpClient&) = delete;
    McpClient& operator=(const McpClient&) = delete;

private:
    // 统一请求发送入口：组装 JSON-RPC 包、分配递增 id、处理错误。
    json send_request(const std::string& method, const json& params);

    // 传输层实现（当前默认使用 HttpTransport）。
    std::unique_ptr<Transport> transport_;
    // 自增请求 id，用于 JSON-RPC request/response 对应。
    int request_id_ = 0;
    // 最近一次错误信息缓存，便于外部拉取。
    std::string last_error_;
};

} // namespace mcp
