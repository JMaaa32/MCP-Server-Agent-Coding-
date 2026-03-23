# MCP Coding Agent

基于自研 C++ MCP Server 的多 Agent 代码助手系统。

## 架构

```
Python Agent（LangGraph）  →  HTTP JSON-RPC  →  C++ MCP Server（13 个工具）
  Coder → Reviewer → Tester                      沙箱执行 / 代码搜索 / 复杂度分析 ...
```

- **LLM**：DeepSeek API（deepseek-chat）
- **编排**：LangGraph 状态图，条件路由，最多 3 轮迭代
- **工具**：read/write_file、execute_code、run_tests、code_search、git_diff、analyze_code 等 13 个
- **安全**：命令白名单 + 引号感知危险符号过滤 + 路径沙箱
- **数据**：自动收集轨迹，导出 SFT / DPO 训练数据

## 快速开始

**1. 编译 MCP Server**

```bash
cmake -B build && cmake --build build -j4
```

**2. 启动 Server**

```bash
./build/src/mcp_server --config config/server.json --port 8089
```

**3. 安装 Python 依赖**

```bash
pip install -r agent/requirements.txt
```

**4. 配置 DeepSeek API Key**

```bash
export DEEPSEEK_API_KEY=your_key_here
```

**5. 运行 Agent**

```bash
# 单次任务
python3 agent/main.py "在 workspace/ 下创建 is_palindrome 函数，写单测，确保通过"

# 持续对话
python3 agent/main.py --chat

# 开启 SSE 实时监控（浏览器访问 http://localhost:8765）
python3 agent/main.py --chat --sse
```

## 目录结构

```
agent/          Python Agent 层（LangGraph 编排）
src/            C++ MCP Server 源码
workspace/      Agent 文件沙箱
trajectories/   轨迹数据输出
docs/plan/      项目文档（规划 / 排错 / 面试准备）
config/         Server 配置
```

## 文档

详见 `[docs/plan/](docs/plan/01-总体规划.md)`，包含完整实施过程、排错记录和面试准备材料。