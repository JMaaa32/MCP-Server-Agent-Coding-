# 05 - DeepSeek API 集成 & 端到端验证

> 阶段二补充：双后端 LLM 支持（DeepSeek API / Ollama 本地）及多 Agent 全流程验证

---

## 背景

本地 Ollama 未安装 `qwen3:8b`，且本地模型的 function calling 稳定性不足。
引入 DeepSeek API（OpenAI 兼容）作为主后端，同时保留 Ollama 作为备选。

---

## 变更文件

### 1. agent/config.py

新增双后端配置：

| 配置项 | 默认值 | 说明 |
|--------|--------|------|
| `LLM_BACKEND` | `"deepseek"` | 通过 `LLM_BACKEND` 环境变量切换，支持 `"deepseek"` 或 `"ollama"` |
| `DEEPSEEK_API_KEY` | 从 `DEEPSEEK_API_KEY` 环境变量读取 | DeepSeek API 密钥 |
| `DEEPSEEK_BASE_URL` | `https://api.deepseek.com` | OpenAI 兼容端点 |
| `DEEPSEEK_MODEL` | `deepseek-chat` | 使用 DeepSeek-V3 |

### 2. agent/llm.py（新增）

统一 LLM 调用封装，核心设计：

- **`chat(messages, tools)`**：根据 `LLM_BACKEND` 分发到 `_chat_deepseek()` 或 `_chat_ollama()`
- **`make_tool_result_messages(assistant_msg, results)`**：关键函数，解决 DeepSeek 和 Ollama 工具调用消息格式差异

**DeepSeek 格式要求**（不满足会返回 400 错误）：
- Assistant 消息中的 `tool_calls` 必须带 `id` 和 `type: "function"` 字段
- `arguments` 必须是 JSON **字符串**（非 dict）
- Tool result 消息必须包含对应的 `tool_call_id`

**Ollama 格式**：
- 简单的 `{"role": "tool", "content": "..."}` 即可，不需要 id

**统一返回格式**：
```python
{
  "message": {
    "role": "assistant",
    "content": "...",
    "tool_calls": [
      {"id": "call_xxx", "function": {"name": "...", "arguments": {...}}}
    ]
  }
}
```

### 3. agent/nodes/*.py（coder.py / reviewer.py / tester.py）

三个 Agent 节点的变更完全一致：
- 移除 `import ollama` 直接调用
- 改为通过 `import llm` 统一调用：`llm.chat(messages, tools)`
- 工具结果回传改用 `llm.make_tool_result_messages(msg, results)`

### 4. agent/requirements.txt

新增依赖：`openai>=1.0.0`（用于 DeepSeek API 调用）

---

## 关键踩坑

### DeepSeek 400 错误

**现象**：第一次工具调用成功，回传结果后 DeepSeek 返回 HTTP 400

**原因**：DeepSeek 严格要求 OpenAI 规范：
1. Assistant 消息的 `tool_calls[].id` 不能为空
2. `tool_calls[].type` 必须是 `"function"`
3. `tool_calls[].function.arguments` 必须是字符串，不能是 dict
4. 每个 `{"role": "tool"}` 消息必须带 `tool_call_id` 且匹配对应的 `tool_calls[].id`

**解决**：创建 `make_tool_result_messages()` 统一处理消息格式

### write_file 沙箱逃逸

**现象**：Agent 调用 `write_file` 时，文件被写到项目根目录而非 `workspace/`

**原因**：`write_file_tool.cpp` 原版无路径限制

**解决**：添加与 `read_file_tool.cpp` 相同的沙箱验证逻辑（`weakly_canonical` + 前缀检查）

---

## 端到端验证结果

### 简化模式（Coder Only）

```bash
cd agent && DEEPSEEK_API_KEY=sk-xxx python main.py --simple "读取 workspace/project_notes.md 并总结内容"
```

| 指标 | 值 |
|------|-----|
| 步骤数 | 2 |
| 工具调用 | 2 |
| 成功率 | 100% |
| 参与 Agent | coder |

### 多 Agent 模式（Coder → Reviewer → Tester）

```bash
cd agent && DEEPSEEK_API_KEY=sk-xxx python main.py "在 workspace/ 下创建 palindrome.py 实现 is_palindrome 函数，写 test_palindrome.py 测试，确保测试通过"
```

| 指标 | 值 |
|------|-----|
| 步骤数 | 34 |
| 工具调用 | 31 |
| 成功率 | 87% |
| 参与 Agent | coder, reviewer, tester |
| 最终结果 | **PASS** |

**Agent 行为亮点**：
- Coder：自主搜索目录结构 → 创建 `palindrome.py`（含 7 种测试用例）→ 创建 `test_palindrome.py`
- Reviewer：读取代码 → 判定 LGTM
- Tester：发现 `pytest` 不可用后，自动切换为 `python3 test_palindrome.py` 直接执行 → 判定 PASS

**产出文件**：
- `workspace/palindrome.py` — `is_palindrome()` 函数
- `workspace/test_palindrome.py` — 7 个测试函数
- `trajectories/trajectory_20260321_133601.json` — 完整轨迹
- `trajectories/trajectory_20260321_133601_sft.json` — SFT 训练数据

---

## 切换后端

```bash
# 使用 DeepSeek（默认）
export LLM_BACKEND=deepseek
export DEEPSEEK_API_KEY=sk-xxx

# 使用 Ollama
export LLM_BACKEND=ollama
# 需先 ollama pull qwen3:8b
```
