/**
 * @file http_jsonrpc.cpp
 * @brief 基于 HTTP 的 JSON-RPC 2.0 服务器实现
 */

#include "http_jsonrpc.h"
#include "logger.h"
#include "config.h"

#include <httplib.h>
#include <nlohmann/json.hpp>

namespace mcp {

// Pimpl 具体实现类
class HttpJsonRpcServer::Impl {
public:
    httplib::Server server;

    Impl() {
        // 日志回调，便于跟踪每个 HTTP 请求
        // server.set_logger([](const httplib::Request& req, const httplib::Response& res) {
        //     MCP_LOG_INFO("HTTP {} {} -> {}", req.method, req.path, res.status);
        // });

        // 全局错误处理，确保遇到「未捕获异常」时返回符合 JSON-RPC 2.0 的错误响应
        server.set_error_handler([](const httplib::Request&, httplib::Response& res) {
            json error_resp = {
                {"jsonrpc", "2.0"},
                {"error", {
                    {"code", -32603},
                    {"message", "Internal server error"}
                }},
                {"id", nullptr}
            };
            res.set_content(error_resp.dump(), "application/json");
        });
    }
};

// 构造函数（使用配置文件）
    // 委托构造函数，调用另一个构造函数
HttpJsonRpcServer::HttpJsonRpcServer(JsonRpcDispatcher dispatcher)
    : HttpJsonRpcServer(std::move(dispatcher), "0.0.0.0", MCP_CONFIG.GetServerPort()) {
    MCP_LOG_INFO("HTTP JSON-RPC server initialized from config");
}

// 构造函数（手动指定参数） 
    // 服务端用途：本文件使用 httplib::Server 搭建 HTTP 入口，注册以下接口：
    // - POST /jsonrpc       ：主入口，将 HTTP 请求体转给内部 JSON-RPC dispatcher 处理，并返回 JSON-RPC 响应。
    // - OPTIONS /jsonrpc    ：CORS 预检，允许浏览器客户端的跨域请求。
    // - GET /health         ：健康检查端点，供运维或负载均衡器探测 HTTP 服务是否健康。
    // - GET /               ：根路径，可用于欢迎页或运行状态检测。
    //
    // 所有具体请求均由内部 JSON-RPC dispatcher 统一处理响应；CORS 相关响应头自动注入所有相关 HTTP 响应。
HttpJsonRpcServer::HttpJsonRpcServer(
    JsonRpcDispatcher dispatcher,
    const std::string& host,
    int port
)
    : dispatcher_(std::move(dispatcher))
    , host_(host)
    , port_(port)
    , impl_(std::make_unique<Impl>())
{
    MCP_LOG_INFO("HTTP JSON-RPC server created on {}:{}", host_, port_);

    // 1) JSON-RPC 主入口：POST /jsonrpc
        // Server 启动时注册路由
    impl_->server.Post("/jsonrpc", [this](const httplib::Request& req, httplib::Response& res) {
        // 给跨域调用准备 CORS 头（例如浏览器前端调用）。
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");

        try {
            
            std::string response = handle_request(req.body);
            res.set_content(response, "application/json");
            res.status = 200;
        } catch (const std::exception& e) {
            MCP_LOG_ERROR("Error handling request: {}", e.what());
            json error_response = {
                {"jsonrpc", "2.0"},
                {"error", {
                    {"code", -32603},
                    {"message", e.what()}
                }},
                {"id", nullptr}
            };
            res.set_content(error_response.dump(), "application/json");
            res.status = 500;
        }
    });

    // 2) CORS 预检入口：OPTIONS /jsonrpc
    // 浏览器发送预检请求时，用于告知允许的方法和请求头。   
    impl_->server.Options("/jsonrpc", [](const httplib::Request& /*req*/, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        res.status = 204;
    });

    // 3) 健康检查：GET /health （GET 辅助端点）
    // 便于监控系统或负载均衡器探测服务是否在线。
    impl_->server.Get("/health", [](const httplib::Request& /*req*/, httplib::Response& res) {
        json health = {
            {"status", "ok"},
            {"service", "mcp-http-jsonrpc"}
        };
        res.set_content(health.dump(), "application/json");
    });

    // 4) 服务信息：GET /  （GET 辅助端点）
    // 返回可用端点列表，方便调试与手工探查。
    impl_->server.Get("/", [this](const httplib::Request& /*req*/, httplib::Response& res) {
        json info = {
            {"service", "MCP HTTP JSON-RPC Server"},
            {"version", "1.0.0"},
            {"endpoints", {
                {{"path", "/jsonrpc"}, {"method", "POST"}, {"description", "JSON-RPC 2.0 endpoint"}},
                {{"path", "/health"}, {"method", "GET"}, {"description", "Health check"}},
                {{"path", "/sse/events"}, {"method", "GET"}, {"description", "Server status event stream (SSE)"}},
                {{"path", "/sse/tool_calls"}, {"method", "GET"}, {"description", "Tool call monitoring stream (SSE)"}},
                {{"path", "/"}, {"method", "GET"}, {"description", "Server information"}}
            }}
        };
        res.set_content(info.dump(2), "application/json");
    });
}

// ***
// 注册 SSE 端点    path:指的是 HTTP 路由路径 ,比如：http://127.0.0.1:8080/sse/events ，
                                            // http://127.0.0.1:8080/sse/tool_calls
  // 把某个 GET 路径注册成 SSE 实时推流接口，并把“如何发消息”封装成 send_event 给业务回调使用
  // 这段代码负责搭好实时通道；真正发什么内容，由外面传入的 callback 决定
void HttpJsonRpcServer::register_sse_endpoint(const std::string& path, SseCallback callback) {
    impl_->server.Get(path, [callback](const httplib::Request& /*req*/, httplib::Response& res) {
        res.set_header("Content-Type", "text/event-stream");
        res.set_header("Cache-Control", "no-cache");
        res.set_header("Connection", "keep-alive");
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("X-Accel-Buffering", "no");

        // 设置 SSE 内容提供者，每次调用都返回一个新连接，业务回调可多次调用 send_event 发消息
            // “边产生边发送”
            // res.set_chunked_content_provider(...) 是 cpp-httplib 提供的 chunked 响应接口
        res.set_chunked_content_provider(
            "text/event-stream",
            [callback](size_t /*offset*/, httplib::DataSink& sink) {
                auto send_event = [&sink](const std::string& data) {
                    std::string event = "data: " + data + "\n\n";
                    // 往 sink.write(...) 写什么，客户端就收到什么
                    sink.write(event.c_str(), event.size());
                };

                try {
                    callback(send_event);
                } catch (const std::exception& e) {
                    MCP_LOG_ERROR("SSE callback error: {}", e.what());
                }

                return true;
            }
        );
    });
}

HttpJsonRpcServer::~HttpJsonRpcServer() {
    stop();
}

// 启动 HTTP 服务（阻塞调用）
void HttpJsonRpcServer::run() {
    // 尝试将 running_ 置为 true，若已在运行则直接返回
    if (running_.exchange(true)) {
        MCP_LOG_WARN("Server is already running");
        return;
    }

    MCP_LOG_INFO("Starting HTTP JSON-RPC server on {}:{}", host_, port_);

    // 启动 HTTP 服务器，listen 为阻塞调用，失败时抛异常
    if (!impl_->server.listen(host_, port_)) {
        running_ = false; // 启动失败重置 running_
        throw std::runtime_error("Failed to start HTTP server on " + host_ + ":" + std::to_string(port_));
    }

    MCP_LOG_INFO("HTTP JSON-RPC server stopped");
    running_ = false; // 服务器停止后重置 running_
}

void HttpJsonRpcServer::stop() {
    // 原子设 running_ 为 false，若此前已是 false，说明服务未在运行，直接返回
    if (!running_.exchange(false)) {
        MCP_LOG_WARN("HTTP JSON-RPC server is not running");
        return;
    }

    MCP_LOG_INFO("Stopping HTTP JSON-RPC server...");

    try {
        // 优雅地停止底层 HTTP 服务器
        impl_->server.stop();
        MCP_LOG_INFO("HTTP JSON-RPC server stopped successfully");
    } catch (const std::exception& e) {
        // 捕获并记录可能出现的异常
        MCP_LOG_ERROR("Failed to stop HTTP JSON-RPC server: {}", e.what());
    }
}

// HTTP -> JSON-RPC 的统一处理入口：
// 1) 解析请求；2) 路由到 dispatcher；3) 组装 JSON-RPC 响应字符串。
std::string HttpJsonRpcServer::handle_request(const std::string& request_body) {
    MCP_LOG_DEBUG("Request body: {}", request_body);

    try {
        json request_json = json::parse(request_body);

        // 轻量解析：把原始 JSON 映射为 JsonRpcRequest 结构体。
        auto parse_jsonrpc_request = [](const json& req_json) -> JsonRpcRequest {
            JsonRpcRequest req;
            req.jsonrpc = req_json.value("jsonrpc", "2.0");
            req.method = req_json.at("method").get<std::string>();
            if (req_json.contains("id")) {
                req.id = req_json["id"];
            }
            if (req_json.contains("params")) {
                req.params = req_json["params"];
            }
            return req;
        };

        // 响应回包：把 JsonRpcResponse 还原成标准 JSON-RPC 响应 JSON。
        auto build_response_json = [](const JsonRpcResponse& resp) -> json {
            json resp_json = {{"jsonrpc", resp.jsonrpc}, {"id", resp.id}};
            if (resp.result.has_value()) {
                resp_json["result"] = *resp.result;
            }
            if (resp.error.has_value()) {
                resp_json["error"] = {
                    {"code", resp.error->code},
                    {"message", resp.error->message}
                };
                if (resp.error->data.has_value()) {
                    resp_json["error"]["data"] = *resp.error->data;
                }
            }
            return resp_json;
        };

        // 处理单条请求（单请求与批量请求都复用此逻辑）。
        auto process_one = [this, &build_response_json](const JsonRpcRequest& req, bool log_notification)
            -> std::optional<json> {
            // Notification（无 id）按规范不返回响应。
            if (!req.id.has_value()) {
                if (dispatcher_.hasHandler(req.method)) {
                    dispatcher_.call(req.method, req.params.value_or(json::object()));
                }
                if (log_notification) {
                    MCP_LOG_DEBUG("Notification request, no response");
                }
                return std::nullopt;
            }

            JsonRpcResponse resp;
            resp.jsonrpc = "2.0";
            resp.id = *req.id;

            try {
                // 路由阶段：method -> Handler，未命中返回 MethodNotFound。
                if (!dispatcher_.hasHandler(req.method)) {
                    resp.error = JsonRpcError{
                        jsonrpc_errc::MethodNotFound,
                        "Method not found: " + req.method,
                        std::nullopt
                    };
                } else {
                    // 命中后执行处理器，处理器结果直接作为 JSON-RPC result。
                    resp.result = dispatcher_.call(req.method, req.params.value_or(json::object()));
                }
            } catch (const std::exception& e) {
                // 业务/处理器异常统一折叠为 InternalError。
                resp.error = JsonRpcError{
                    jsonrpc_errc::InternalError,
                    e.what(),
                    std::nullopt
                };
            }

            return build_response_json(resp);
        };

        if (request_json.is_array()) {
            // 批量请求：逐条处理并收集可返回项（通知不会产生返回项）。
            // JSON-RPC 2.0: 空批量请求是无效请求，必须返回 Invalid Request 错误对象。
            if (request_json.empty()) {
                json error_response = {
                    {"jsonrpc", "2.0"},
                    {"error", {
                        {"code", jsonrpc_errc::InvalidRequest},
                        {"message", "Invalid Request"}
                    }},
                    {"id", nullptr}
                };
                return error_response.dump();
            }

            json batch_response = json::array();
            for (const auto& single_req_json : request_json) {
                try {
                    JsonRpcRequest req = parse_jsonrpc_request(single_req_json);
                    auto resp_json = process_one(req, false);
                    if (resp_json.has_value()) {
                        batch_response.push_back(*resp_json);
                    }
                } catch (const std::exception& e) {
                    MCP_LOG_ERROR("Error in batch request: {}", e.what());
                    json error_resp = {
                        {"jsonrpc", "2.0"},
                        {"error", {
                            {"code", jsonrpc_errc::InternalError},
                            {"message", e.what()}
                        }},
                        {"id", nullptr}
                    };
                    batch_response.push_back(error_resp);
                }
            }

            // JSON-RPC 2.0: 批量中若全是通知，请求不应返回响应体。
            if (batch_response.empty()) {
                return "";
            }

            std::string response = batch_response.dump();
            MCP_LOG_DEBUG("Batch response: {}", response);
            return response;
        }

        // 单请求路径：解析 -> 处理 ->（若非通知）返回响应体。
        JsonRpcRequest req = parse_jsonrpc_request(request_json);
        auto response_json = process_one(req, true);
        if (!response_json.has_value()) {
            return "";
        }
        return response_json->dump();

    } catch (const json::parse_error& e) {
        // 入参 JSON 本身非法：返回 ParseError。
        MCP_LOG_ERROR("JSON parse error: {}", e.what());
        json error_response = {
            {"jsonrpc", "2.0"},
            {"error", {
                {"code", jsonrpc_errc::ParseError},
                {"message", std::string("Parse error: ") + e.what()}
            }},
            {"id", nullptr}
        };
        return error_response.dump();
    } catch (const std::exception& e) {
        // 兜底异常：返回 InternalError。
        MCP_LOG_ERROR("Error handling request: {}", e.what());
        json error_response = {
            {"jsonrpc", "2.0"},
            {"error", {
                {"code", jsonrpc_errc::InternalError},
                {"message", std::string("Internal error: ") + e.what()}
            }},
            {"id", nullptr}
        };
        return error_response.dump();
    }
}

} // namespace mcp
