#include "jsonrpc_serialization.h"
#include <stdexcept>

namespace mcp {

// =========================
// JsonRpcRequest
// =========================
void to_json(json& j, const JsonRpcRequest& r) {
    j = json{{"jsonrpc", "2.0"}, {"method", r.method}};
    if (r.id.has_value()) {
        j["id"] = *r.id;
    }
    if (r.params.has_value()) {
        j["params"] = *r.params;
    }
}

void from_json(const json& j, JsonRpcRequest& r) {
    r.jsonrpc = j.at("jsonrpc").get<std::string>();
    r.method = j.at("method").get<std::string>();
    if (j.contains("id")) {
        r.id = j.at("id");
    } else {
        r.id.reset();
    }
    if (j.contains("params")) {
        r.params = j.at("params");
    } else {
        r.params.reset();
    }
}

void to_json(json& j, const JsonRpcError& e) {
    j = json{{"code", e.code}, {"message", e.message}};
    if (e.data) {
        j["data"] = *e.data;
    }
}

/// 从 JSON 解析 JsonRpcError
void from_json(const json& j, JsonRpcError& e) {
    e.code = j.at("code").get<int>();
    e.message = j.at("message").get<std::string>();
    if (j.contains("data")) {
        e.data = j.at("data");
    } else {
        e.data.reset();
    }
}


void to_json(json& j, const JsonRpcResponse& r) {
    j = json{{"jsonrpc", "2.0"}, {"id", r.id}};

    // JSON-RPC 2.0: result 和 error 必须二选一
    if (r.error.has_value() && r.result.has_value()) {
        throw std::invalid_argument("Invalid JSON-RPC response: both result and error are set");
    }
    if (!r.error.has_value() && !r.result.has_value()) {
        throw std::invalid_argument("Invalid JSON-RPC response: neither result nor error is set");
    }

    if (r.error.has_value()) {
        j["error"] = *r.error;
    } else {
        j["result"] = *r.result;
    }
}

/// 从 JSON 解析 JsonRpcResponse
void from_json(const json& j, JsonRpcResponse& r) {
    r.jsonrpc = j.at("jsonrpc").get<std::string>();

    if (!j.contains("id")) {
        throw std::invalid_argument("Invalid JSON-RPC response: missing id");
    }
    r.id = j.at("id");

    const bool has_error = j.contains("error");
    const bool has_result = j.contains("result");
    if (has_error == has_result) {
        throw std::invalid_argument("Invalid JSON-RPC response: result and error must be mutually exclusive");
    }

    if (has_error) {
        r.error = j.at("error").get<JsonRpcError>();
        r.result.reset();
    } else {
        r.result = j.at("result");
        r.error.reset();
    }
}

} // namespace mcp
