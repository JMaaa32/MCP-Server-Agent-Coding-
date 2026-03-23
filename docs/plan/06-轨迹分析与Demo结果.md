# 06 - 轨迹分析与训练数据质量评估

> 阶段三产出：批量轨迹分析工具 + SFT/DPO 数据质量评估 + Demo 任务扩展

---

## 概述

在完成多 Agent 系统搭建和端到端验证（阶段二）后，本阶段聚焦于：

1. **轨迹质量评估**：量化分析每条执行轨迹的效率、正确性和训练价值
2. **批量分析工具**：`agent/analyze.py`，支持批量统计、质量分级、报告导出
3. **DPO 配对生成**：自动从多条轨迹中构造 chosen/rejected 对比数据
4. **Demo 任务扩展**：运行 Bug 修复、代码理解等多样化任务，积累训练数据

---

## 新增文件

### agent/analyze.py — 轨迹分析工具

**用法**：

```bash
cd agent

# 分析所有轨迹并打印报告
python analyze.py --report

# 生成 DPO 训练数据
python analyze.py --export-dpo

# 保存报告 JSON
python analyze.py --save-report

# 指定轨迹目录
python analyze.py --dir trajectories/ --report --export-dpo --save-report
```

---

## 分析维度

### 单轨迹指标

| 指标 | 计算方式 | 说明 |
|------|---------|------|
| `total_steps` | 所有步骤数 | 包含工具调用和纯消息 |
| `tool_calls` | 工具调用步骤数 | 排除 message 类型 |
| `tool_errors` | 工具调用失败数 | `is_error = true` 的步骤 |
| `tool_success_rate` | `1 - errors/calls` | 工具调用成功率 |
| `redundant_calls` | 连续相同工具+相同参数 | 检测重复无效调用 |
| `error_recoveries` | 错误后成功重试次数 | Agent 自主恢复能力 |
| `efficiency_score` | 综合评分 (0-1) | 错误率×0.3 + 冗余率×0.3 + 步数惩罚×0.4 |

### 效率评分计算

```
efficiency = 1.0 - error_rate × 0.3 - redundancy_rate × 0.3 - step_penalty × 0.4

其中:
  error_rate = tool_errors / tool_calls
  redundancy_rate = redundant_calls / tool_calls
  step_penalty = max(0, (tool_calls - 10) / 20)  # 超过 10 步开始扣分
```

### SFT 数据质量分级

| 等级 | 条件 | 说明 |
|------|------|------|
| **good** | 成功 + efficiency > 0.6 | 高质量，可直接用于 SFT |
| **usable** | 成功但 efficiency ≤ 0.6 | 可用但有噪声（步数多/有冗余调用） |
| **rejected** | 失败 | 不用于 SFT，可作为 DPO 的 rejected |

---

## DPO 配对策略

`generate_dpo_pairs()` 支持三种配对方式：

| 策略 | 配对条件 | chosen | rejected |
|------|---------|--------|----------|
| success_vs_failure | 同任务，一成一败 | 成功轨迹 | 失败轨迹 |
| efficient_vs_inefficient | 同任务，都成功 | 步数最少的 | 步数最多的 |
| （待扩展）cross_task | 不同任务 | 高效成功轨迹 | 低效/失败轨迹 |

输出格式：

```json
[
  {
    "prompt": "任务描述",
    "chosen": "Thought: ...\nAction: ...\n...",
    "rejected": "Thought: ...\nAction: ...\n...",
    "pair_type": "success_vs_failure"
  }
]
```

---

## Demo 任务扩展

### 全部 Demo 结果

| # | 任务类型 | 描述 | 后端 | 结果 | 步数 | 工具成功率 | SFT 质量 |
|---|---------|------|------|------|------|----------|---------|
| 1 | 代码生成 | 创建 palindrome.py + 测试 | DeepSeek | **PASS** | 34 | 87% | usable |
| 2 | Bug 修复 | 修复 buggy.py sort_list + find_max | DeepSeek | **PASS** | 33 | 83% | usable |
| 3 | 代码理解 | 分析 .py 文件，生成 API 文档 | DeepSeek | **FAIL** | 143 | 93% | rejected |
| 4 | 代码生成 | fibonacci 三种实现 + 测试 | DeepSeek | **PASS** | 46 | 95% | usable |

**分析**：
- Demo 3 失败原因：任务过于开放（"分析所有文件+生成文档"），Agent 在 3 轮迭代中都无法满足 Tester 要求
- 工具调用热点：`execute_code`(83 次) > `read_file`(73) > `code_search`(40)
- 分号过滤改进后 `execute_code` 成功率显著提升（Demo 4 达到 95%）

---

## 分析报告示例

```
              Overview
┏━━━━━━━━━━━━━━━━━━━━━━━┳━━━━━━━━┓
┃ Metric                ┃  Value ┃
┡━━━━━━━━━━━━━━━━━━━━━━━╇━━━━━━━━┩
│ Total trajectories    │      4 │
│ Success rate          │ 100.0% │
│ Avg steps             │   34.0 │
│ Avg tool success rate │  87.1% │
│ Avg efficiency        │  54.1% │
└───────────────────────┴────────┘

 SFT Data Quality
┏━━━━━━━━━┳━━━━━━━┓
┃ Quality ┃ Count ┃
┡━━━━━━━━━╇━━━━━━━┩
│ usable  │     1 │
└─────────┴───────┘
```

---

## 量化目标 vs 实际

| 指标 | 目标 | 当前（4 条轨迹） |
|------|------|---------|
| Demo 任务完成率 | ≥ 80% | 75% (3/4) ⚠️ |
| 平均完成步数 | ≤ 8 步 | 64 步 ❌ |
| 工具调用成功率 | ≥ 95% | 89.7% ⚠️ |
| 多 Agent 迭代收敛 | ≤ 3 轮 | 1-3 轮 ✅ |
| SFT/DPO 可导出 | ✓ | ✓ ✅ |
| DPO 配对数 | ≥ 1 | 已生成 ✅ |

**已完成的改进**：
- ✅ `execute_code` 引号感知分号检测 → Demo 4 工具成功率提升到 95%
- ✅ Ollama 改用 requests 绕过 httpx 代理 502 问题
- ✅ DeepSeek/Ollama 调用加重试机制

**待改进**：
- 步数偏多（64 步 vs 目标 8 步）：Agent 倾向于过度探索（反复 read_file + code_search），需优化 System Prompt 减少冗余调用
- Demo 3 失败：任务描述过于开放，需要给更明确的输出规范
- Reviewer/Tester 循环效率低：3 轮迭代 ×15 步 = 最多 135 步，考虑降低 MAX_TOOL_CALLS 或增加 early stop

---

## 运行前提

1. C++ MCP Server 运行中
2. 已有轨迹文件在 `agent/trajectories/` 目录
3. Python 依赖已安装：`pip install rich`
