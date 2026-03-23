# 12. Claude Desktop MCP 接入排错记录

**日期**：2026-03-23
**负责人**：[协作]（用户操作 + AI 分析修复）
**目标**：让 Claude Desktop 通过 stdio 模式调用自研 C++ MCP Server 的工具

---

## 问题总览

| # | 问题 | 现象 | 根因 | 修复位置 |
|---|------|------|------|---------|
| 1 | verify 脚本硬编码路径 | `❌ mcp_server 不存在` | 路径写死为 `/Users/chef/...` | `scripts/verify_claude_config.sh` |
| 2 | macOS 无 `timeout` 命令 | 脚本步骤 5 报 exit 127 | macOS 不带 GNU coreutils | `scripts/verify_claude_config.sh` |
| 3 | claude_desktop_config 缺少 mcpServers | 工具未加载，用联网搜索代替 | 配置文件只有 preferences，没有 mcpServers | `~/Library/Application Support/Claude/claude_desktop_config.json` |
| 4 | 日志路径相对路径导致 crash | Server disconnected（启动即崩溃） | `../../logs/server.log` 在 Claude Desktop 启动时 CWD=`/`，路径解析失败 | `config/server.json` |
| 5 | 日志写入 stdout 污染 MCP 流 | JSON 解析错误 position 3 | `stdout_color_sink_mt` 写到 fd1，与 MCP stdio 共用同一管道 | `config/server.json`（关闭 console 日志） |
| 6 | stdio 协议不匹配（NDJSON vs LSP） | initialize 请求 60 秒超时 | Claude Desktop 发裸 JSON 行，Server 只认 `Content-Length` 头 | `src/json_rpc/stdio_jsonrpc.cpp` |
| 7 | workspace 路径依赖 CWD | 写文件报 workspace 不存在 | 7 个工具都用 `current_path()/"workspace"`，Claude Desktop 启动时 CWD 不是项目根 | 7 个 tool cpp + config + `config.h/cpp` |
| 8 | execute_code 无超时 | 工具调用卡死不返回 | `popen` 无超时机制 | `src/server/builtin/tools/execute_code_tool.cpp` |

---

## 详细排错过程

### 问题 1 & 2：verify 脚本失效

**现象**：运行 `sh scripts/verify_claude_config.sh` 报路径错误，步骤 3 和步骤 5 失败。

**根因**：
- 第 9 行：`MCP_SERVER="/Users/chef/Documents/..."` 写死了别人的路径
- macOS 没有 `timeout` 命令（是 GNU coreutils 的东西），直接返回 exit 127

**修复** [AI]：
```bash
# 改为动态推导
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
MCP_SERVER="$PROJECT_ROOT/build/src/mcp_server"

# 用后台进程 + kill -0 替代 timeout
"$MCP_SERVER" --config "$CONFIG_JSON" --mode stdio < /dev/null > /tmp/mcp_test_out.txt 2>&1 &
MCP_PID=$!
sleep 2
if kill -0 $MCP_PID 2>/dev/null; then
    kill $MCP_PID 2>/dev/null
    echo "✅ mcp_server 可以正常启动"
fi
```

---

### 问题 3：Claude Desktop 没有加载 MCP 工具

**现象**：问"查一下北京的天气"，Claude Desktop 用联网搜索而不是 MCP 工具。

**排查**：打印 `claude_desktop_config.json`，发现只有 preferences，完全没有 `mcpServers` 字段。

**修复** [AI]：
```json
{
  "mcpServers": {
    "mcp-coding-agent": {
      "command": "/Users/mac/Desktop/CS/Cplusplus/mcp-tutorial/build/src/mcp_server",
      "args": ["--config", "/Users/mac/.../config/server.json", "--mode", "stdio"]
    }
  },
  "preferences": { ... }
}
```

---

### 问题 4：Server 启动即 Crash（日志路径）

**现象**：Claude Desktop 日志显示：
```
libc++abi: terminating due to uncaught exception:
filesystem error: in create_directories: Read-only file system ["../../logs"]
```

**根因**：`config/server.json` 里 `log_file_path: "../../logs/server.log"` 是相对路径。
Claude Desktop 从系统目录（`/`）启动 server，`../../logs` 解析到只读文件系统。

**修复** [AI]：改为绝对路径
```json
"log_file_path": "/Users/mac/Desktop/CS/Cplusplus/mcp-tutorial/logs/server.log"
```

---

### 问题 5：日志写入 stdout 污染 MCP 流

**现象**：Claude Desktop 报 `Expected ',' or ']' after array element in JSON at position 3`

**排查**：手动测试发现即使 `2>/dev/null` 重定向 stderr，日志仍然出现在 stdout：
```bash
printf "..." | ./mcp_server --mode stdio 2>/dev/null | xxd
# 输出第一行：[15:56:35.896] [info] [mcp_server] Logger initialized...
```

**根因**：`src/logger/logger.cpp` 使用 `spdlog::sinks::stdout_color_sink_mt`，写到 fd1（stdout），而 MCP stdio 通信也用 fd1，两者混在一起。

**修复** [AI]：最直接的修法是在 config 关掉 console 日志（运行期生效，无需重编译）：
```json
"log_console_output": false
```
同时也将源码中 `stdout_color_sink_mt` 改为 `stderr_color_sink_mt`（需重编译）。

---

### 问题 6：stdio 协议不匹配（核心问题）

**现象**：initialize 请求发出后 60 秒超时，server 日志显示：
```
Ignore header: {"method": "initialize","params":{...},"jsonrpc":"2.0","id":0}
```

**根因**：协议格式不匹配。

| 端 | 期望格式 |
|---|---------|
| 我们的 Server（旧） | LSP 格式：`Content-Length: N\r\n\r\n{JSON}` |
| Claude Desktop（MCP 2025-11-25） | NDJSON：`{JSON}\n` |

Server 把整个 JSON 行当成 header 里的一个字段，找不到 `Content-Length`，就跳过了。

**修复** [AI]：同时支持两种格式，NDJSON 优先：

```cpp
// readMessage：行首是 { 或 [ 直接当消息体
if (!line.empty() && (line.front() == '{' || line.front() == '[')) {
    out_body = std::move(line);
    return true;
}

// writeMessage：改为 NDJSON
void StdioJsonRpcServer::writeMessage(const json& msg) {
    out_ << msg.dump() << "\n";
    out_.flush();
}
```

---

### 问题 7：workspace 路径依赖 CWD

**现象**：write_file / read_file 等工具报 `Error: workspace/ directory does not exist`

**根因**：7 个工具全部使用：
```cpp
fs::canonical(fs::current_path() / "workspace")
```
`current_path()` 在 Claude Desktop 启动时是 `/`，不是项目根目录，`/workspace` 不存在。

**修复** [AI]：
1. `config.h` 加 `GetWorkspaceRoot()` 接口
2. `config.cpp` 实现（读 `workspace_root` 字段）
3. `config/server.json` 加绝对路径：
   ```json
   "workspace_root": "/Users/mac/Desktop/CS/Cplusplus/mcp-tutorial/workspace"
   ```
4. 7 个工具改为：
   ```cpp
   const std::string cfg = MCP_CONFIG.GetWorkspaceRoot();
   fs::path root = cfg.empty() ? fs::current_path() / "workspace" : fs::path(cfg);
   fs::create_directories(root, ec);  // 自动创建，不再报错
   root = fs::weakly_canonical(root);
   ```

**受影响文件**：
- `write_file_tool.cpp`
- `read_file_tool.cpp`
- `list_files_tool.cpp`
- `run_tests_tool.cpp`
- `execute_code_tool.cpp`
- `code_search_tool.cpp`
- `analyze_code_tool.cpp`

---

### 问题 8：execute_code 无超时卡死

**现象**：Claude Desktop 调用 execute_code 执行 `/tmp/maze test1.txt`，进程卡死不返回，整个对话冻结。

**根因**：`popen` 是阻塞调用，没有任何超时机制，子进程等待输入或死循环时会永久阻塞。

**修复** [AI]：用 `select` + 轮询实现 30 秒超时：
```cpp
int fd = fileno(pipe);
auto start = std::chrono::steady_clock::now();

while (true) {
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - start).count();
    if (elapsed >= TIMEOUT_SECS) { timed_out = true; break; }

    fd_set fds; FD_ZERO(&fds); FD_SET(fd, &fds);
    struct timeval tv{1, 0};
    int ready = select(fd + 1, &fds, nullptr, nullptr, &tv);
    if (ready == 0) continue;

    ssize_t n = read(fd, buffer.data(), buffer.size() - 1);
    if (n <= 0) break;
    // ... 读取输出
}

if (timed_out)
    output += "\n[killed: command timed out after 30s]";
```

---

## 编译方式

由于 vcpkg 目录缺失导致 cmake 无法重新 configure，所有重编译通过脚本直接调用编译器：

```bash
bash scripts/rebuild.sh
```

脚本路径：`scripts/rebuild.sh`
输出：`build/src/mcp_server`

**注意**：每次修改 C++ 源码后都需要运行此脚本，然后重启 Claude Desktop 生效。

---

## 最终配置状态

**`config/server.json`**：
```json
{
  "workspace_root": "/Users/mac/Desktop/CS/Cplusplus/mcp-tutorial/workspace",
  "server": { "port": 8080 },
  "logging": {
    "log_file_path": "/Users/mac/Desktop/CS/Cplusplus/mcp-tutorial/logs/server.log",
    "log_level": "debug",
    "log_file_size": 52428800,
    "log_file_count": 5,
    "log_console_output": false
  }
}
```

**`claude_desktop_config.json`**：
```json
{
  "mcpServers": {
    "mcp-coding-agent": {
      "command": "/Users/mac/Desktop/CS/Cplusplus/mcp-tutorial/build/src/mcp_server",
      "args": ["--config", "/Users/mac/Desktop/CS/Cplusplus/mcp-tutorial/config/server.json", "--mode", "stdio"]
    }
  }
}
```

---

## 可复现步骤

1. 修改代码后执行 `bash scripts/rebuild.sh`
2. 重启 Claude Desktop（Command+Q 退出，再重开）
3. 验证：`bash scripts/verify_claude_config.sh`
4. 调试日志：`tail -f logs/server.log`
5. Claude Desktop 日志：`tail -f ~/Library/Logs/Claude/mcp-server-mcp-coding-agent.log`
