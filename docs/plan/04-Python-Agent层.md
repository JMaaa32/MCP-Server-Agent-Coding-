# 04 - Python Agent 层参考文档

> 阶段二产出：LangGraph 多 Agent 编排系统的设计与实现说明

---

## 概述

基于 LangGraph 构建多 Agent 编排系统，通过 MCP HTTP 协议调用 C++ 工具层，实现"代码生成 → 审查 → 测试"的自动化闭环。支持 DeepSeek API（OpenAI 兼容）和 Ollama 本地模型双后端。同时集成轨迹收集模块，可导出 SFT/DPO 训练数据。

---

## 目录结构

```
agent/
├── main.py                 ← CLI 入口（多 Agent / 简化模式）
├── graph.py                ← LangGraph 状态图定义
├── config.py               ← 全局配置（Server URL / LLM 后端 / 控制参数）
├── llm.py                  ← 统一 LLM 调用封装（DeepSeek API / Ollama）
├── mcp_client.py           ← MCP HTTP Client（JSON-RPC 2.0）
├── tool_adapter.py         ← MCP Tool → Ollama function calling 适配
├── trajectory.py           ← 轨迹收集 + SFT/DPO 导出
├── nodes/
│   ├── coder.py            ← Coder Agent 节点
│   ├── reviewer.py         ← Reviewer Agent 节点
│   └── tester.py           ← Tester Agent 节点
├── tests/
├── requirements.txt
└── __init__.py
```

---

## 模块说明

### 1. mcp_client.py — MCP HTTP Client

**职责**：封装与 C++ MCP Server 的 JSON-RPC 2.0 通信。

**类 `McpHttpClient`**:

| 方法 | 说明 |
|------|------|
| `health_check() → bool` | 检查 Server 是否可用 |
| `list_tools() → list[dict]` | 获取工具列表（含 name/description/inputSchema） |
| `call_tool(name, args) → dict` | 调用工具，返回原始 MCP 结果 |
| `call_tool_parsed(name, args) → (str, bool)` | 调用工具并解析，返回 (text, is_error) |

**健壮性设计**:

- 统一超时：15 秒（可配置）
- 自动重试：网络失败重试 1 次，业务错误不重试
- 绕过系统代理：`proxies={"http": "", "https": ""}`
- 统一异常：`McpToolError`

---

### 2. tool_adapter.py — 工具格式适配

**职责**：将 MCP Tool Schema 转为 Ollama function calling 格式。

**核心函数**:

| 函数 | 说明 |
|------|------|
| `mcp_tool_to_ollama(tool) → dict` | 单个 MCP Tool → Ollama 格式 |
| `get_ollama_tools(client) → list[dict]` | 从 Server 拉取全部工具并转换 |
| `get_tools_description(client) → str` | 生成文本描述（用于 System Prompt） |
| `filter_tools_for_role(tools, role) → list[dict]` | 按角色过滤可用工具 |

**角色工具权限**:

| 角色 | 可用工具 |
|------|---------|
| coder | read_file, write_file, code_search, list_files, execute_code |
| reviewer | read_file, code_search, list_files |
| tester | run_tests, read_file, execute_code |

---

### 3. graph.py — LangGraph 状态图

**State 结构 `CodingAgentState`**:

```python
class CodingAgentState(TypedDict):
    task: str                   # 用户任务描述
    messages: list              # 当前对话历史
    generated_code: str         # Coder 的产出摘要
    review_feedback: str        # Reviewer 的反馈（"LGTM" 或修改建议）
    test_results: dict          # {"passed": bool, "feedback": str}
    iteration: int              # 当前 Coder→Reviewer→Tester 轮次
    trajectory: TrajectoryCollector  # 轨迹收集器
    final_answer: str
```

**状态流转**:

```
         ┌──────────────────────────────────────┐
         │                                      │
         ▼                                      │
      coder ──→ reviewer ──→ tester ─── PASS ──→ END
                                │
                                ├── FAIL + iteration < 3 ──→ coder (retry)
                                │
                                └── FAIL + iteration ≥ 3 ──→ END
```

**关键函数**:

| 函数 | 说明 |
|------|------|
| `build_graph(mcp_client) → StateGraph` | 构建状态图，注入 MCP 客户端 |
| `run_coding_agent(task, client) → (answer, trajectory)` | 执行完整流程 |

---

### 4. nodes/ — 三个 Agent 节点

#### coder.py — Coder Agent

- **System Prompt 核心指令**：先搜索上下文（code_search/list_files），再生成代码（write_file），最后验证语法（execute_code）
- **ReAct 循环**：调用 Ollama → 处理 tool_calls → 回传结果 → 继续，直到模型不再调用工具
- **迭代支持**：第 0 轮接收原始任务；第 1+ 轮接收 Reviewer 反馈和测试失败信息
- **上限**：单节点最多 15 次工具调用

#### reviewer.py — Reviewer Agent

- **审查维度**：正确性、边界处理、代码质量、潜在 bug
- **输出格式**：以 `VERDICT: LGTM` 或 `VERDICT: NEEDS_REVISION` 结尾
- **解析逻辑**：包含 "LGTM" → `review_feedback = "LGTM"`；否则保留完整反馈文本

#### tester.py — Tester Agent

- **测试方式**：优先用 `run_tests` 执行 pytest/gtest；无测试文件时用 `execute_code` 直接运行验证
- **输出格式**：`VERDICT: PASS` 或 `VERDICT: FAIL` + 失败分析
- **结果写入**：`test_results = {"passed": bool, "feedback": str}`

---

### 5. trajectory.py — 轨迹收集器

**类 `TrajectoryCollector`**:

**记录方法**:

| 方法 | 说明 |
|------|------|
| `record_step(agent, thought, action, args, observation, is_error)` | 记录工具调用步骤 |
| `record_agent_message(agent, message)` | 记录纯文本消息 |
| `finish(success, final_answer)` | 标记任务完成 |

**导出方法**:

| 方法 | 输出格式 | 说明 |
|------|---------|------|
| `export_trajectory_json()` | 完整轨迹 JSON | 调试 + 分析用 |
| `export_sft_format()` | `[{instruction, input, output}]` | 仅成功轨迹 → SFT 训练 |
| `export_dpo_pairs(other)` | `[{prompt, chosen, rejected}]` | 成功 vs 失败配对 → DPO 训练 |
| `compute_metrics()` | `{total_steps, tool_calls, tool_success_rate, ...}` | 统计指标 |

**实时展示**（rich 库）:

```
╭──────────────────────────── Step 1: Coder Agent ─────────────────────────────╮
│ Thought  Need to read file                                                   │
│ Action   read_file                                                           │
│ Args     {"path": "x.py"}                                                    │
│ Result   OK file content...                                                  │
╰──────────────────────────────────────────────────────────────────────────────╯
```

---

### 6. main.py — CLI 入口

**用法**:

```bash
# 多 Agent 模式（默认）
python main.py "在 workspace/ 下创建 is_palindrome 函数，写单测，确保通过"

# 简化模式（仅 Coder）
python main.py --simple "读取 workspace/project_notes.md 并总结"

# 指定 Server 和模型
python main.py --server http://localhost:8089/jsonrpc --model qwen3:8b "任务"

# 不导出轨迹
python main.py --no-export "任务"
```

**输出**:

- 控制台：实时 rich 面板 + 最终结果 + 指标摘要
- 文件：`trajectories/trajectory_YYYYMMDD_HHMMSS.json`（完整轨迹）
- 文件：`trajectories/trajectory_YYYYMMDD_HHMMSS_sft.json`（SFT 数据，仅成功时）

---

### 7. llm.py — 统一 LLM 调用封装

**职责**：封装 DeepSeek API 和 Ollama 本地模型的调用差异，提供统一接口。

| 函数 | 说明 |
|------|------|
| `chat(messages, tools) → dict` | 根据 `LLM_BACKEND` 分发到对应后端 |
| `make_tool_result_messages(assistant_msg, results) → list` | 生成工具结果回传消息（处理 DeepSeek/Ollama 格式差异） |

> 详见 [05-deepseek-integration.md](./05-deepseek-integration.md)

---

### 8. config.py — 全局配置

| 配置项 | 默认值 | 说明 |
|--------|--------|------|
| `MCP_SERVER_URL` | `http://localhost:8089/jsonrpc` | C++ MCP Server 地址 |
| `MCP_REQUEST_TIMEOUT` | 15 | 请求超时（秒） |
| `MCP_MAX_RETRIES` | 1 | 网络失败重试次数 |
| `LLM_BACKEND` | `"deepseek"` | LLM 后端：`"deepseek"` 或 `"ollama"` |
| `DEEPSEEK_API_KEY` | 环境变量 | DeepSeek API 密钥 |
| `DEEPSEEK_BASE_URL` | `https://api.deepseek.com` | DeepSeek 端点 |
| `DEEPSEEK_MODEL` | `deepseek-chat` | DeepSeek 模型名 |
| `OLLAMA_BASE_URL` | `http://localhost:11434` | Ollama 地址 |
| `OLLAMA_MODEL` | `qwen3:8b` | Ollama 模型名 |
| `MAX_ITERATIONS` | 3 | Coder→Reviewer→Tester 最大循环轮次 |
| `MAX_TOOL_CALLS` | 15 | 单节点最大工具调用次数 |
| `TRAJECTORY_DIR` | `trajectories` | 轨迹输出目录 |

---

## 依赖

```
langgraph>=0.2.0
langchain-core>=0.3.0
ollama>=0.4.0
openai>=1.0.0
requests>=2.31.0
rich>=13.0.0
```

---

## 验证结果

| 测试项 | 结果 |
|--------|------|
| `McpHttpClient.health_check()` | True |
| `McpHttpClient.list_tools()` | 12 个工具 |
| `McpHttpClient.call_tool_parsed("read_file", ...)` | size=915，正常返回 |
| `get_ollama_tools()` 格式转换 | 12 个 Ollama 格式工具 |
| `filter_tools_for_role("coder")` | 5 个工具（read_file/write_file/code_search/list_files/execute_code） |
| `filter_tools_for_role("reviewer")` | 3 个工具（read_file/code_search/list_files） |
| `filter_tools_for_role("tester")` | 3 个工具（run_tests/read_file/execute_code） |
| `TrajectoryCollector` 记录 + 导出 | rich 面板显示正常，metrics/SFT JSON 导出正常 |
| Python import 全模块 | 全部通过（兼容 Python 3.9+） |

---

## 运行前提

1. C++ MCP Server 运行中：`./build/src/mcp_server --config config/server.json --port 8089`
2. **DeepSeek 模式**（默认）：设置 `DEEPSEEK_API_KEY` 环境变量
3. **Ollama 模式**（备选）：`ollama serve` + `ollama pull qwen3:8b` + `export LLM_BACKEND=ollama`
