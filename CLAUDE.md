1. 每有新增和优化都要文档说明 — 每步产出文档，编号放在 docs/plan/   
2. 面试指南永远放最后编号 — 不固定编号，始终是最大的那个      
3. 面试题格式 —                                               
先写怎么回答（面试口述版），再写知识点详解（含代码示例）
4. 子编号分文件 — 11.1、11.2 这种，详细拆分不要堆在一个文件
5. 可追溯可复现 —
每步都要写清楚是谁做的（[我]/[AI]/[协作]），之后能跟着再做一遍
6. 分工明晰 — 哪些是 AI 做的，哪些是人做的，所有步骤都要可见

# 项目规范

## 构建与运行
- 构建：cmake -B build && cmake --build build -j$(nproc)
- 测试：cd build && ctest --output-on-failure
- 格式化：clang-format -i src/**/*.{cpp,h}
- 静态分析：cppcheck --enable=all src/

## 代码规范
- 标准：C++17，禁止裸 new/delete，用智能指针
- 错误处理：用 Result<T, Error> 类型，禁止裸 throw 到上层
- 日志：spdlog，必须包含 requestId / sessionId
- 禁止：魔法数字、硬编码路径、在头文件 include 不必要的依赖

## 安全硬规则（IMPORTANT）
- 所有外部输入必须验证后使用
- 文件操作必须路径沙箱隔离
- 命令执行必须白名单过滤
- 禁止把内部错误详情暴露给外部调用方

## 测试
- 框架：GTest
- 覆盖要求：每个 public 方法必须有单测，覆盖 happy path + 主要错误路径
- 不允许没有测试的 PR

## 架构约定
- 分层：Application → Service → Repository，禁止跨层调用
- 模块间依赖通过抽象接口，不依赖具体实现
- 新工具注册统一走 ToolRegistry，禁止硬编码工具列表

## 常见陷阱
- 单测运行前需要 export PROJECT_ROOT=$(pwd)
- vcpkg 依赖变更后需要重新 cmake configure
- Sanitizer 模式：cmake -DENABLE_ASAN=ON