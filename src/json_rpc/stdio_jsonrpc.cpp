/**
 * @file stdio_jsonrpc.cpp
 * @brief 基于 stdio 的 JSON-RPC 2.0 服务器
 * 通过标准输入输出通信，使用 Content-Length 头分隔消息
 */

#include "jsonrpc.h"
#include "jsonrpc_serialization.h"
#include "logger.h"

#include <iostream>
#include <sstream>
#include <limits>
#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace mcp {

// ============================================================================
// JsonRpcDispatcher - 方法调度器
// ============================================================================

/// 注册 RPC 方法处理器
void JsonRpcDispatcher::registerHandler(const std::string& method, Handler handler) {
    handlers_[method] = std::move(handler); 
}

/// 检查方法是否已注册
bool JsonRpcDispatcher::hasHandler(const std::string& method) const {
    return handlers_.find(method) != handlers_.end();
}

/// 调用已注册的方法
json JsonRpcDispatcher::call(const std::string& method, const json& params) const {
    auto it = handlers_.find(method);
    if (it == handlers_.end()) {
        throw std::runtime_error("Method not found");
    }
    return it->second(params);
}

// ============================================================================
// StdioJsonRpcServer
// ============================================================================

StdioJsonRpcServer::StdioJsonRpcServer(JsonRpcDispatcher dispatcher)
    : dispatcher_(std::move(dispatcher)) {}

StdioJsonRpcServer::StdioJsonRpcServer(JsonRpcDispatcher dispatcher, std::istream& in, std::ostream& out)
    : dispatcher_(std::move(dispatcher)), in_(in), out_(out) {}

// 读取一条完整的 JSON-RPC 消息
// 支持两种格式：
//   1. LSP 格式：Content-Length: N\r\n\r\n{JSON}
//   2. NDJSON 格式：{JSON}\n（Claude Desktop 使用此格式）
// 成功返回 true，失败返回 false。
bool StdioJsonRpcServer::readMessage(std::string& out_body) {
    out_body.clear();

    std::string line;
    size_t content_length = 0;
    bool found_content_length = false;

    // 读取第一行，判断格式
    while (std::getline(in_, line)) {
        // 去除 Windows 风格的 \r（CR）
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        // NDJSON 格式：行以 { 或 [ 开头，直接当作消息体
        if (!line.empty() && (line.front() == '{' || line.front() == '[')) {
            out_body = std::move(line);
            return true;
        }

        // 空行为头部结束
        if (line.empty())
            break;

        auto colon = line.find(':');
        if (colon == std::string::npos)
            continue; // 忽略非法头部

        std::string key = line.substr(0, colon);
        std::string value = line.substr(colon + 1);
        // 去掉 value 前导空格
        size_t vpos = value.find_first_not_of(' ');
        if (vpos != std::string::npos)
            value = value.substr(vpos);

        // key 转小写以实现大小写不敏感
        std::transform(key.begin(), key.end(), key.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        if (key == "content-length") {
            try {
                content_length = static_cast<size_t>(std::stoul(value));
            } catch (...) {
                MCP_LOG_ERROR("Invalid Content-Length: {}", value);
                return false;
            }
            found_content_length = true;
        } else if (key == "content-type") {
            MCP_LOG_DEBUG("Content-Type: {}", value);
        } else {
            MCP_LOG_DEBUG("Ignore header: {}", line);
        }
    }

    if (!found_content_length)
        return false;

    // 允许空消息体
    if (content_length == 0)
        return true;

    // 按长度精确读取消息体（不是 readline，因为文本中可以有换行符）
    out_body.resize(content_length);
    size_t total_read = 0;
    while (total_read < content_length) {
        in_.read(&out_body[total_read], static_cast<std::streamsize>(content_length - total_read));
        std::streamsize just_read = in_.gcount();
        if (just_read <= 0)
            break;
        total_read += static_cast<size_t>(just_read);
        if (!in_.good() && !in_.eof())
            break;
    }

    if (total_read != content_length) {
        MCP_LOG_ERROR("Incomplete message: expected {} bytes, got {}", content_length, total_read);
        return false;
    }

    return true;
}

/// 写入消息（NDJSON 格式：单行 JSON + 换行）
void StdioJsonRpcServer::writeMessage(const json& msg) {
    out_ << msg.dump() << "\n";
    out_.flush();
}

/**
 * 处理单个 JSON-RPC 请求
 * 错误码: -32700(Parse)、-32600(Invalid)、-32601(NotFound)、-32602(Params)、-32603(Internal)
 */
JsonRpcResponse StdioJsonRpcServer::handleRequest(const JsonRpcRequest& req) {
    JsonRpcResponse resp;
    resp.jsonrpc = "2.0";
    resp.id = req.id.has_value() ? *req.id : nullptr;

    try {
        // 检查 jsonrpc 协议版本和 method 字段
        if (req.jsonrpc != "2.0") {
            throw std::invalid_argument("Invalid Request: jsonrpc must be 2.0");
        }
        if (req.method.empty()) {
            throw std::invalid_argument("Invalid Request: method is missing");
        }

        const std::string& method = req.method;
        json params = req.params.value_or(json::object());

        MCP_LOG_DEBUG("Handling method: {}", method);

        // 检查方法是否注册
        if (!dispatcher_.hasHandler(method)) {
            resp.error = JsonRpcError{
                jsonrpc_errc::MethodNotFound,
                "Method not found",
                std::nullopt
            };
            resp.result.reset();
            return resp;
        }

        // 业务处理
        try {
            json result = dispatcher_.call(method, params);
            resp.result = std::move(result);
            resp.error.reset();
        } catch (const std::invalid_argument& ex) {
            // 参数异常
            resp.result.reset();
            resp.error = JsonRpcError{
                jsonrpc_errc::InvalidParams,
                ex.what(),
                std::nullopt
            };
        } catch (const std::exception& ex) {
            // 业务内部异常
            resp.result.reset();
            resp.error = JsonRpcError{
                jsonrpc_errc::InternalError,
                ex.what(),
                std::nullopt
            };
        }
    } catch (const std::invalid_argument& ex) {
        // 请求格式错误
        resp.result.reset();
        resp.error = JsonRpcError{
            jsonrpc_errc::InvalidRequest,
            ex.what(),
            std::nullopt
        };
    } catch (const json::parse_error& ex) {
        // JSON 解析异常
        resp.result.reset();
        resp.error = JsonRpcError{
            jsonrpc_errc::ParseError,
            ex.what(),
            std::nullopt
        };
    } catch (const std::exception& ex) {
        // 未知异常
        resp.result.reset();
        resp.error = JsonRpcError{
            jsonrpc_errc::InternalError,
            ex.what(),
            std::nullopt
        };
    }

    return resp;
}

// 阻塞运行：循环读取请求并输出响应 
void StdioJsonRpcServer::run() {
    MCP_LOG_INFO("JSON-RPC stdio server starting...");

    // 用来装每次读到的请求体
    std::string request_body;

    while (true) {
        // 读取请求
        if (!readMessage(request_body)) {
            if (in_.eof()) { // 说明输入结束
                MCP_LOG_INFO("stdin EOF reached, exiting");
                break;
            }
            continue; // 跳过这轮继续等
        }

        // 解析并处理
        try {
            // 解析 JSON
                // - 如果 JSON 语法错误，会进外层 catch (json::parse_error)
            json j = json::parse(request_body);

            // 尝试解析成请求对象
                // - 如果结构不合法，抛异常，会进内层 catch (std::exception)
            JsonRpcRequest req;
            try {
                // nlohmann/json 的 get<T>() 方法会自动解析 JSON 并转换为 T 类型
                req = j.get<JsonRpcRequest>();
            } catch (const std::exception& ex) {
                // 构造 InvalidRequest(-32600)
                JsonRpcResponse resp;
                resp.id = j.contains("id") ? j["id"] : nullptr;
                resp.error = JsonRpcError{jsonrpc_errc::InvalidRequest, ex.what(), std::nullopt};
                // 写回后 continue 处理下一条
                writeMessage(json(resp));
                continue;
            }

            // 处理请求
            // 在 JSON-RPC 2.0 里：
                // - 有 id：是请求（request），客户端期待响应
                // - 没 id：是通知（notification），客户端不期待响应
            const bool is_notification = !req.id.has_value();  // 通知无需响应
            auto resp = handleRequest(req);

            if (!is_notification) {
                writeMessage(json(resp));
            }
        } catch (const json::parse_error& ex) {
            // 单独捕获 JSON 解析错误
            MCP_LOG_ERROR("JSON parse error: {}", ex.what());
            JsonRpcResponse resp;
            resp.id = nullptr;
            resp.error = JsonRpcError{jsonrpc_errc::ParseError, ex.what(), std::nullopt};
            writeMessage(json(resp));
        }
        
    }
}

} // namespace mcp
