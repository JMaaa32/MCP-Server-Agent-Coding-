/**
 * @file types.h
 * @brief MCP 协议核心类型定义
 *
 * 包含 Tools、Resources、Prompts 等核心类型
 */

#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <optional>
#include <variant>

namespace mcp {

using json = nlohmann::json;

// MCP 协议版本
constexpr const char* LATEST_PROTOCOL_VERSION = "2024-11-05";
constexpr const char* DEFAULT_NEGOTIATED_VERSION = "2024-11-05";

// 进度令牌类型
using ProgressToken = std::variant<std::string, int64_t>;

// 光标类型（用于分页）
using Cursor = std::string;

// 角色类型
enum class Role {
    User,
    Assistant
};

// ----------------------------------------
// Tool 相关（4 个类型）
// ----------------------------------------

// 工具输入 Schema (JSON Schema)
    // 描述工具接受什么参数 （标准 JSON Schema 格式）
struct ToolInputSchema {
    std::string type = "object";
    json properties;
    std::vector<std::string> required;

    json to_json() const;
    static ToolInputSchema from_json(const json& j);
};

// 工具定义
    // 工具的"名片"——名称、描述、参数规范
struct Tool {
    std::string name;                      // 工具名称
    std::string description;               // 工具描述
    ToolInputSchema input_schema;          // 输入 Schema

    json to_json() const;
    static Tool from_json(const json& j);
};

// 工具调用结果内容（文本或图片等）
    // 结果的一个内容块，支持三种类型： 纯文本、图片 、 资源引用 url
struct ContentItem {
    std::string type;      // "text", "image", "resource"
    std::optional<std::string> text;
    std::optional<std::string> data;       // base64 for image
    std::optional<std::string> mime_type;
    std::optional<std::string> uri;        // for resource

    json to_json() const;
    static ContentItem from_json(const json& j);
};

// 工具调用结果
    // 最终返回值，多个 ContentItem + 是否出错
struct ToolResult {
    std::vector<ContentItem> content;
    bool is_error = false;

    json to_json() const;
    static ToolResult from_json(const json& j);
};


// ----------------------------------------
// Resource 相关（2 个类型）
// ----------------------------------------

// 资源定义
    // 资源的一个"名片"——URI、名称、描述、MIME 类型
struct Resource {
    std::string uri;                           // 资源 URI (如 file:///path/to/file)
    std::string name;                          // 资源名称
    std::optional<std::string> description;    // 资源描述
    std::optional<std::string> mime_type;      // MIME 类型

    json to_json() const;
    static Resource from_json(const json& j);
};

// 资源内容
    // 资源的实际内容，支持文本（text）和二进制（blob ：base64 编码的二进制内容）
struct ResourceContent {
    std::string uri;
    std::optional<std::string> mime_type;
    std::string text;           // 文本内容
    std::optional<std::string> blob;  // base64 编码的二进制内容

    json to_json() const;
    static ResourceContent from_json(const json& j);
};


// ----------------------------------------
// Prompt 相关（3 个类型）
// ----------------------------------------

// 提示参数
    // 提示参数的"名片"——名称、描述、是否必填
struct PromptArgument {
    std::string name;
    std::optional<std::string> description;
    bool required = false;

    json to_json() const;
    static PromptArgument from_json(const json& j);
};

// 提示定义
    // 提示模板定义（名称 + 参数列表）
struct Prompt {
    std::string name;
    std::optional<std::string> description;
    std::vector<PromptArgument> arguments;

    json to_json() const;
    static Prompt from_json(const json& j);
};

// 提示消息
    // 生成后的消息（角色 + 内容），最终组成对话上下文
struct PromptMessage {
    Role role;
    json content;

    json to_json() const;
    static PromptMessage from_json(const json& j);
};


// ----------------------------------------
// 服务初始化（3 个类型）
// ----------------------------------------

// 服务器能力
    // 声明服务器支持哪些能力（tools/resources/prompts），以及是否支持 list_changed 通知 和 资源订阅
struct ServerCapabilities {
    // 工具能力
    struct ToolsCapability {
        bool list_changed = false; // 工具列表是否发生变化

        json to_json() const; // 转换为 JSON
        static ToolsCapability from_json(const json& j); // 从 JSON 转换为 ToolsCapability
    };

    // 资源能力
    struct ResourcesCapability {
        bool subscribe = false; // 是否订阅资源
        bool list_changed = false; // 资源列表是否发生变化

        json to_json() const; // 转换为 JSON
        static ResourcesCapability from_json(const json& j); // 从 JSON 转换为 ResourcesCapability
    };

    // 提示能力
    struct PromptsCapability {
        bool list_changed = false; // 提示列表是否发生变化  

        json to_json() const; // 转换为 JSON
        static PromptsCapability from_json(const json& j); // 从 JSON 转换为 PromptsCapability
    };

    std::optional<ToolsCapability> tools; // 工具能力
    std::optional<ResourcesCapability> resources; // 资源能力
    std::optional<PromptsCapability> prompts; // 提示能力
    std::optional<json> logging; // 日志配置

    json to_json() const; // 转换为 JSON
    static ServerCapabilities from_json(const json& j);
};


// MCP 协议服务器信息
    // 服务器名称和版本
struct ServerInfo {
    std::string name; // 服务器名称
    std::string version; // 服务器版本

    json to_json() const;
    static ServerInfo from_json(const json& j);
};

// MCP 协议初始化结果
    // MCP 握手时返回的完整初始化响应：初始化结果，包含协议版本、服务器能力和服务器信息
struct InitializeResult {
    std::string protocol_version; // MCP 协议版本
    ServerCapabilities capabilities; // 服务器能力
    ServerInfo server_info; // 服务器信息

    json to_json() const;
    static InitializeResult from_json(const json& j);
};

} // namespace mcp
