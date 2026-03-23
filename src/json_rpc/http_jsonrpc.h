#pragma once

#include "jsonrpc.h"
#include <string>
#include <functional>
#include <memory>
#include <atomic>

namespace mcp {

class HttpJsonRpcServer {
public:
 
    explicit HttpJsonRpcServer(JsonRpcDispatcher dispatcher);

    HttpJsonRpcServer(
        JsonRpcDispatcher dispatcher,
        const std::string& host,
        int port
    );

    ~HttpJsonRpcServer();

    // 禁止拷贝
    HttpJsonRpcServer(const HttpJsonRpcServer&) = delete;
    HttpJsonRpcServer& operator=(const HttpJsonRpcServer&) = delete;


    // 启动 HTTP 服务（阻塞调用）。
    void run();
    // 请求停止服务器监听。
    void stop();

    bool is_running() const { return running_.load(); }

    // 获取监听主机地址（构造时设置）。
    const std::string& get_host() const { return host_; }

    // 获取监听端口（构造时设置）。
    int get_port() const { return port_; }

    // SSE 回调：参数 send(data) 用于向客户端推送一条事件。
    using SseCallback = std::function<void(const std::function<void(const std::string&)>&)>;

    // 在服务器上注册 SSE GET 路由（如 "/sse/events"）。
    // 通常由启动代码调用；客户端访问该路径后会持续接收事件。
    void register_sse_endpoint(const std::string& path, SseCallback callback);

private:
    JsonRpcDispatcher dispatcher_;
    std::string host_;
    int port_;
    std::atomic<bool> running_{false};

    class Impl;
    std::unique_ptr<Impl> impl_;

    std::string handle_request(const std::string& request_body);
};

} // namespace mcp
