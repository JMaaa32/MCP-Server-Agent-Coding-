/**
 * @file mcp_client.cpp
 * @brief MCP 客户端实现
 */


#include "mcp_client.h"
#include <httplib.h>
#include <stdexcept>

namespace mcp {

// ===== HttpTransport 实现 =====
class HttpTransport::Impl {
public:
    Impl(const std::string& host, int port)
        : host_(host), port_(port), client_(host, port) {
        client_.set_connection_timeout(5, 0);  // 5 秒连接超时
        client_.set_read_timeout(30, 0);       // 30 秒读取超时
    }

    // 发送 JSON-RPC 请求，返回服务端 JSON 响应 
    json send(const json& request) { 
        // ① 发client_.Post 是 cpp-httplib 提供的 HTTP POST 方法，
        //      把 JSON 对象 dump() 成字符串，往 /jsonrpc 端点发出去
        // ② 等 httplib::Result res 是阻塞等待，Server 处理完才返回，
        //      期间调用方卡在这行不动。
        httplib::Result res = client_.Post("/jsonrpc", request.dump(), "application/json");

        //③ 收拿到响应后做两个检查：
            // 连不上 Server（没启动 / 端口错了）
        if (!res) {
            throw std::runtime_error("Failed to connect to MCP server at " +
                                   host_ + ":" + std::to_string(port_));
        }
            // Server 返回了 HTTP 错误
        if (res->status != 200) {
            throw std::runtime_error("HTTP error: " + std::to_string(res->status));
        }
            // 如果 Server 返回了 JSON 字符串，用 json::parse() 解析成 JSON 对象。
        try {
            return json::parse(res->body);
        } catch (const json::exception& e) {
            throw std::runtime_error("Failed to parse JSON response: " + std::string(e.what()));
        }
    }

private:
    std::string host_;
    int port_;
    httplib::Client client_;
};

HttpTransport::HttpTransport(const std::string& host, int port)
    : impl_(std::make_unique<Impl>(host, port)) {}

HttpTransport::~HttpTransport() = default;

json HttpTransport::send(const json& request) {
    return impl_->send(request);
}

// McpClient 实现
McpClient::McpClient(const std::string& host, int port) : transport_(std::make_unique<HttpTransport>(host, port)) {}

McpClient::~McpClient() = default;

json McpClient::send_request(const std::string& method, const json& params) {
    last_error_.clear();

    // ① 组装 JSON-RPC 请求
    try {
        json request = {
            {"jsonrpc", "2.0"},
            {"method", method},
            {"params", params},
            {"id", ++request_id_}
        };

        // ② 发送 http 请求，返回服务端 JSON 响应 
            // 往 localhost:xxxx /jsonrpc 发 POST，等 C++ Server 处理完返回 JSON。
        json response = transport_->send(request);

        // ③  解包结果 / 处理错误
            // 从响应里取出 result 字段返回给调用方，如果是 error 字段就抛异常。
        if (response.contains("error")) {
            last_error_ = response["error"]["message"].get<std::string>();
            throw std::runtime_error("MCP error: " + last_error_);
        }

        return response["result"];

    } catch (const std::exception& e) {
        last_error_ = e.what();
        throw;
    }
}

InitializeResult McpClient::initialize() {
    json result = send_request("initialize", json::object());
    return InitializeResult::from_json(result);
}

std::vector<Tool> McpClient::list_tools() {
    json result = send_request("tools/list", json::object());

    std::vector<Tool> tools;
    for (const auto& tool_json : result["tools"]) {
        tools.push_back(Tool::from_json(tool_json));
    }
    return tools;
}

ToolResult McpClient::call_tool(const std::string& name, const json& arguments) {
    json params = {
        {"name", name},
        {"arguments", arguments}
    };

    json result = send_request("tools/call", params);
    return ToolResult::from_json(result);
}

std::vector<Resource> McpClient::list_resources() {
    json result = send_request("resources/list", json::object());

    std::vector<Resource> resources;
    for (const auto& res_json : result["resources"]) {
        resources.push_back(Resource::from_json(res_json));
    }
    return resources;
}

ResourceContent McpClient::read_resource(const std::string& uri) {
    json params = {{"uri", uri}};
    json result = send_request("resources/read", params);

    // 返回第一个内容项
    return ResourceContent::from_json(result["contents"][0]);
}

std::vector<Prompt> McpClient::list_prompts() {
    json result = send_request("prompts/list", json::object());

    std::vector<Prompt> prompts;
    for (const auto& prompt_json : result["prompts"]) {
        prompts.push_back(Prompt::from_json(prompt_json));
    }
    return prompts;
}

std::vector<PromptMessage> McpClient::get_prompt(const std::string& name, const json& arguments) {
    json params = {
        {"name", name},
        {"arguments", arguments}
    };

    json result = send_request("prompts/get", params);

    std::vector<PromptMessage> messages;
    for (const auto& msg_json : result["messages"]) {
        messages.push_back(PromptMessage::from_json(msg_json));
    }
    return messages;
}

std::string McpClient::get_last_error() const {
    return last_error_;
}

} // namespace mcp
