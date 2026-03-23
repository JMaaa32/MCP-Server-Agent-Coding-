#include "http_jsonrpc.h"
#include "logger.h"
#include "mcp_server.h"
#include "config.h"
#include "server/builtin/builtin_registry.h"

#include <spdlog/spdlog.h>
#include <iostream>
#include <csignal>
#include <atomic>
#include <memory>
#include <vector>
#include <mutex>
#include <thread>
#include <chrono>
#include <ctime>
#include <condition_variable>

using namespace mcp;

// 全局服务器实例指针（用于信号处理）
static std::atomic<bool> g_running{true};
static std::unique_ptr<HttpJsonRpcServer> g_http_server{nullptr};

// 信号处理函数
void signal_handler(int signal) {
    std::cerr << "\nReceived signal " << signal << ", shutting down..." << std::endl;
    g_running = false;
    if (g_http_server) {
        g_http_server->stop();
    }
}

// 字符串转日志级别
spdlog::level::level_enum StringToLogLevel(const std::string& level_str) {
    if (level_str == "trace") {   
        return spdlog::level::trace;
    } else if (level_str == "debug") {
        return spdlog::level::debug;
    } else if (level_str == "info") {
        return spdlog::level::info;
    } else if (level_str == "warn") {
        return spdlog::level::warn;
    } else if (level_str == "error") {
        return spdlog::level::err;
    } else if (level_str == "critical") {
        return spdlog::level::critical;
    }
    // Default to info if unknown
    return spdlog::level::info;
}

// 注册 MCP 工具、资源、提示词
void setup_mcp_server(McpServer& mcp) {
    register_builtin_components(mcp);
}

// 创建 JSON-RPC 调度器（绑定到 MCP 服务器）
    // 注册一个 JSON-RPC 调度器，把 MCP 服务器的能力暴露给外部调用。
    // 绑定到 MCP 服务器，方便后续调用 MCP 服务器的能力
JsonRpcDispatcher create_dispatcher(McpServer& mcp_server) {
    JsonRpcDispatcher dispatcher;

    // initialize
    dispatcher.registerHandler("initialize", [&mcp_server](const json& /*params*/) -> json {
        MCP_LOG_INFO("Client initialized");
        return mcp_server.get_initialize_result().to_json();
    });

    // tools/list
    dispatcher.registerHandler("tools/list", [&mcp_server](const json& /*params*/) -> json {
        json tools_arr = json::array();
        for (const auto& tool : mcp_server.list_tools()) {
            tools_arr.push_back(tool.to_json());
        }
        return {{"tools", tools_arr}};
    });

    // tools/call
    dispatcher.registerHandler("tools/call", [&mcp_server](const json& params) -> json {
        std::string name = params.at("name").get<std::string>();
        json arguments = params.value("arguments", json::object());
        MCP_LOG_INFO("Calling tool: {}", name);
        auto result = mcp_server.call_tool(name, arguments);
        return result.to_json();
    });

    // resources/list
    dispatcher.registerHandler("resources/list", [&mcp_server](const json& /*params*/) -> json {
        json resources_arr = json::array();
        for (const auto& resource : mcp_server.list_resources()) {
            resources_arr.push_back(resource.to_json());
        }
        return {{"resources", resources_arr}};
    });

    // resources/read
    dispatcher.registerHandler("resources/read", [&mcp_server](const json& params) -> json {
        std::string uri = params.at("uri").get<std::string>();
        MCP_LOG_INFO("Reading resource: {}", uri);
        auto content = mcp_server.read_resource(uri);
        json contents_arr = json::array();
        contents_arr.push_back(content.to_json());
        return {{"contents", contents_arr}};
    });

    // prompts/list
    dispatcher.registerHandler("prompts/list", [&mcp_server](const json& /*params*/) -> json {
        json prompts_arr = json::array();
        for (const auto& prompt : mcp_server.list_prompts()) {
            prompts_arr.push_back(prompt.to_json());
        }
        return {{"prompts", prompts_arr}};
    });

    // prompts/get
    dispatcher.registerHandler("prompts/get", [&mcp_server](const json& params) -> json {
        std::string name = params.at("name").get<std::string>();
        json arguments = params.value("arguments", json::object());
        MCP_LOG_INFO("Getting prompt: {}", name);
        auto messages = mcp_server.get_prompt(name, arguments);
        json messages_arr = json::array();
        for (const auto& msg : messages) {
            messages_arr.push_back(msg.to_json());
        }
        return {{"messages", messages_arr}};
    });

    return dispatcher;
}

// HTTP 模式
void run_http_mode(McpServer& mcp_server, const std::string& host, int port) {
    MCP_LOG_INFO("Starting HTTP server on {}:{}", host, port);

    auto dispatcher = create_dispatcher(mcp_server);
    g_http_server = std::make_unique<HttpJsonRpcServer>(std::move(dispatcher), host, port);

    // 线程安全事件队列：回调写入，SSE 端点读取并发送
    struct EventQueue {
        std::vector<json> events;
        std::mutex mutex;
        std::condition_variable cv;
    } event_queue;

    // MCP 事件进入队列，供 /sse/tool_calls 实时消费
    mcp_server.set_sse_callback([&event_queue](const json& event) {
        std::lock_guard<std::mutex> lock(event_queue.mutex);
        event_queue.events.push_back(event);
        event_queue.cv.notify_all();
    });

    // SSE: 推送周期性服务器状态
    g_http_server->register_sse_endpoint("/sse/events", [&mcp_server](const auto& send) {
        MCP_LOG_INFO("SSE events client connected");
        send(json({{"type", "connected"}, {"message", "Server events stream"}}).dump());

        int uptime_seconds = 0;
        while (g_running.load()) {
            const json status = {
                {"type", "server_status"},
                {"timestamp", std::time(nullptr)},
                {"tools_count", mcp_server.list_tools().size()},
                {"resources_count", mcp_server.list_resources().size()},
                {"prompts_count", mcp_server.list_prompts().size()},
                {"uptime_seconds", uptime_seconds++}
            };
            send(status.dump());
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }

        MCP_LOG_INFO("SSE events client disconnected");
    });

    // SSE: 推送工具调用等异步事件
    g_http_server->register_sse_endpoint("/sse/tool_calls", [&event_queue](const auto& send) {
        MCP_LOG_INFO("SSE tool_calls client connected");
        send(json({{"type", "connected"}, {"message", "Tool calls monitoring"}}).dump());

        while (g_running.load()) {
            std::unique_lock<std::mutex> lock(event_queue.mutex);

            // 等待新事件，超时后继续循环检查 g_running
            event_queue.cv.wait_for(lock, std::chrono::seconds(1), [&event_queue] {
                return !event_queue.events.empty();
            });

            for (const auto& event : event_queue.events) {
                send(event.dump());
            }
            event_queue.events.clear();
        }

        MCP_LOG_INFO("SSE tool_calls client disconnected");
    });

    // 阻塞运行，直到 stop() 或进程退出
    g_http_server->run();
    MCP_LOG_INFO("HTTP server stopped");
}

// stdio 模式
    // 按当前实现，StdioJsonRpcServer 通常不需要额外线程安全保护
    // 前提是它按“单线程事件循环”使用。
void run_stdio_mode(McpServer& mcp_server) {
    MCP_LOG_INFO("Starting stdio server");

    auto dispatcher = create_dispatcher(mcp_server);
    StdioJsonRpcServer stdio_server(std::move(dispatcher));

    // 启动服务器（阻塞）
    stdio_server.run();

    MCP_LOG_INFO("stdio server stopped");
}

// 同时运行两种模式
void run_both_modes(McpServer& mcp_server, const std::string& host, int port) {
    MCP_LOG_INFO("Starting both HTTP and stdio servers");

    // 在独立线程中运行 HTTP 服务器
    std::thread http_thread([&mcp_server, &host, port]() {
        run_http_mode(mcp_server, host, port);
    });

    // 在主线程运行 stdio 服务器
    run_stdio_mode(mcp_server);

    // 等待 HTTP 线程结束
    if (http_thread.joinable()) {
        http_thread.join();
    }
}

int main(int argc, char* argv[]) {
    // 解析命令行参数
    std::string config_file = "../../config/server.json";
    std::string mode = "http"; // 默认 HTTP 模式
    std::string host;
    int port = 0;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--config" && i + 1 < argc) {
            config_file = argv[++i];
        } else if (arg == "--mode" && i + 1 < argc) {
            mode = argv[++i];
        } else if (arg == "--host" && i + 1 < argc) {
            host = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            try {
                port = std::stoi(argv[++i]);
            } catch (const std::exception&) {
                std::cerr << "Invalid --port value: " << argv[i] << std::endl;
                return 1;
            }
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [OPTIONS]\n"
                      << "Options:\n"
                      << "  --mode MODE      Server mode: http, stdio, or both (default: http)\n"
                      << "  --config FILE    Configuration file path\n"
                      << "  --host HOST      Server host (for HTTP mode)\n"
                      << "  --port PORT      Server port (for HTTP mode)\n"
                      << "  --help, -h       Show this help\n"
                      << "\nExamples:\n"
                      << "  " << argv[0] << " --mode http --port 8080\n"
                      << "  " << argv[0] << " --mode stdio\n"
                      << "  " << argv[0] << " --mode both --port 8080\n";
            return 0;
        }
    }

    // 验证模式参数
    if (mode != "http" && mode != "stdio" && mode != "both") {
        std::cerr << "Invalid mode: " << mode << " (must be http, stdio, or both)" << std::endl;
        return 1;
    }

    // 加载配置（失败时继续跑默认值/命令行值）
        // 优先级：命令行参数 > 配置文件 > 代码默认值
    if (!MCP_CONFIG.LoadFromFile(config_file)) {
        std::cerr << "Failed to load config from: " << config_file << std::endl;
    }

    if (host.empty()) {
        host = "0.0.0.0";
    }
    if (port == 0) {
        port = MCP_CONFIG.GetServerPort();
    }

    // 初始化日志
    MCP_LOG_INIT("mcp_server", MCP_CONFIG.GetLogFilePath(),
                 MCP_CONFIG.GetLogFileSize(), MCP_CONFIG.GetLogFileCount(),
                 MCP_CONFIG.GetLogConsoleOutput());
    MCP_LOG_SET_LEVEL(StringToLogLevel(MCP_CONFIG.GetLogLevel()));

    if (mode == "http" || mode == "both") {
        MCP_LOG_INFO("HTTP: {}:{}", host, port);
    }

    try {
        // 创建 MCP 服务器实例
        McpServer mcp_server("mcp-server", "1.0.0");

        // 声明服务能力
        ServerCapabilities capabilities;
        capabilities.tools = ServerCapabilities::ToolsCapability{false};
        capabilities.resources = ServerCapabilities::ResourcesCapability{false, false};
        capabilities.prompts = ServerCapabilities::PromptsCapability{false};
        mcp_server.set_capabilities(capabilities);

        // *** 注册内置 Tools/Resources/Prompts
        setup_mcp_server(mcp_server);

        // 注册退出信号
        std::signal(SIGINT, signal_handler);
        std::signal(SIGTERM, signal_handler);

        // 按模式启动
        if (mode == "http") {
            run_http_mode(mcp_server, host, port);
        } else if (mode == "stdio") {
            run_stdio_mode(mcp_server);
        } else { // both
            run_both_modes(mcp_server, host, port);
        }

        MCP_LOG_INFO("Server shutdown complete");
    } catch (const std::exception& e) {
        MCP_LOG_ERROR("Fatal error: {}", e.what());
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }

    MCP_LOG_SHUTDOWN();
    return 0;
}