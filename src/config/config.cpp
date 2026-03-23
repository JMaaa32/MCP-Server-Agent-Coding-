#include "config.h"
#include <fstream>
#include <iostream>

namespace mcp {

// 4) 将 getter 改为从 config_data_ 读取真实值，不再返回占位默认值。
// 5) 增加/更新 tests/test_config.cpp，覆盖成功、失败、边界场景。

Config& Config::GetInstance() {
    static Config instance;
    return instance;
}

// 打开文件、解析 JSON、异常处理、设置 loaded_
bool Config::LoadFromFile(const std::string& config_file_path) {
    config_file_path_ = config_file_path;
    try {
        std::ifstream file(config_file_path_);

        file >> config_data_;
        file.close();

        if (!ValidateConfig()) {
            std::cerr << "Invalid config" << std::endl;
            return false;
        }

        SetDefaults(); // 为缺失字段设置默认值

        loaded_ = true;
        return true;

    } catch (const json::parse_error& e) {
        std::cerr << "Config parse error: " << e.what() << std::endl;
        return false;
    } catch (const std::exception& e) {
        std::cerr << "Error loading config: " << e.what() << std::endl;
        return false;
    }
}

// 校验 server/logging 等字段合法性。
bool Config::ValidateConfig() const {
    // 校验 server 配置存在
    if (!config_data_.contains("server")) {
        std::cerr << "Missing 'server' section in config" << std::endl;
        return false;
    }

    // 校验端口范围
    int port = config_data_["server"].value("port", 8080);
    if (port < 1 || port > 65535) {
        std::cerr << "Server port must be in range 1-65535: " << port << std::endl;
        return false;
    }

    // 校验 logging 配置（如存在）
    if (config_data_.contains("logging")) {
        std::string log_level = config_data_["logging"].value("log_level", std::string("info"));
        if (log_level != "trace" && log_level != "debug" && log_level != "info" &&
            log_level != "warn" && log_level != "error" && log_level != "critical") {
            std::cerr << "Invalid log level: " << log_level << std::endl;
            std::cerr << "Valid levels: trace, debug, info, warn, error, critical" << std::endl;
            return false;
        }

        size_t log_file_size = config_data_["logging"].value("log_file_size", 10 * 1024 * 1024);
        if (log_file_size == 0) {
            std::cerr << "Log file size must be greater than 0" << std::endl;
            return false;
        }

        int log_file_count = config_data_["logging"].value("log_file_count", 5);
        if (log_file_count <= 0) {
            std::cerr << "Log file count must be greater than 0" << std::endl;
            return false;
        }
    }

    // 🔧 扩展时: 可在此增加新字段的合法性校验

    return true;
}

void Config::SetDefaults() {
    // 为缺失字段设置默认值
    if (!config_data_.contains("server")) {
        config_data_["server"] = json::object();
    }

    auto& server = config_data_["server"];

    if (!server.contains("port")) {
        server["port"] = 8080;  // Default port
    }

    if (!config_data_.contains("logging")) {
        config_data_["logging"] = json::object();
    }

    auto& logging = config_data_["logging"];

    if (!logging.contains("log_file_path")) {
        logging["log_file_path"] = "../../logs/server.log";
    }

    if (!logging.contains("log_level")) {
        logging["log_level"] = "info";
    }

    if (!logging.contains("log_file_size")) {
        logging["log_file_size"] = 10 * 1024 * 1024;  // 10MB
    }

    if (!logging.contains("log_file_count")) {
        logging["log_file_count"] = 5;
    }

    if (!logging.contains("log_console_output")) {
        logging["log_console_output"] = true;
    }
}

// ===================================================================
// Config getter implementations
// ===================================================================

int Config::GetServerPort() const {
    return config_data_["server"].value("port", 8080);
}

std::string Config::GetLogFilePath() const {
    return config_data_["logging"].value("log_file_path", std::string("logs/server.log"));
}

std::string Config::GetLogLevel() const {
    return config_data_["logging"].value("log_level", std::string("info"));
}

size_t Config::GetLogFileSize() const {
    return config_data_["logging"].value("log_file_size", 10 * 1024 * 1024); // Default 10MB
}

int Config::GetLogFileCount() const {
    return config_data_["logging"].value("log_file_count", 5);
}

bool Config::GetLogConsoleOutput() const {
    return config_data_["logging"].value("log_console_output", true);
}

std::string Config::GetWorkspaceRoot() const {
    return config_data_.value("workspace_root", "");
}

} // namespace mcp
