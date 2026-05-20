# Optix 开发者文档

本文档面向二次开发者，重点说明如何在不破坏内核稳定性的前提下扩展 `Optix`。

## 1. 开发环境准备

## 1.1 前置要求

- CMake `>= 3.20`
- 支持 C++20 的编译器
- Clang LibTooling/libclang 开发包（严格必需）

## 1.2 构建与测试

```bash
./build.sh --debug
./build.sh --release --test
```

手动方式：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## 1.3 严格模式说明

- 项目强制 `OPTIX_ENABLE_LIBTOOLING=ON`。
- 若 CMake 无法找到可用 Clang 包或链接目标，会直接失败。
- 工程不会调用 `clang++` 二进制命令来做 AST 提取。

## 2. 代码结构与职责

```text
include/optix/
  core/        # 核心类型、上下文、产物接口
  stages/      # 阶段抽象接口
  plugins/     # 插件 ABI 与管理接口

lib/optix/
  core/        # 内核实现（编排、事件、序列化、插件加载）
  stages/      # ingest/normalize/diagnose/index/rule/suggest
  plugins/     # 默认插件实现

tools/optix/
  main.cpp     # CLI 命令入口
```

## 3. 核心扩展点

## 3.1 新增规则（推荐优先）

适用场景：新增性能模式识别、优化建议细化。

步骤：
1. 在 `rules/` 增加或修改 YAML 规则。
2. 在 `lib/optix/plugins/rulepack/` 补充匹配与评分逻辑。
3. 为新规则补充正反例测试。
4. 执行全量测试并观察建议噪声率。

质量门槛：
- 每条规则至少 1 个正例和 1 个反例。
- 建议必须可定位到文件与行号。
- 高风险建议默认降级为仅报告。

## 3.2 新增 Parser 插件

适用场景：接入新指标源（非 ksys）。

要求：
- 把新来源统一转换为 `RawMetricRecord`。
- 保持字段语义和单位一致，必要时在 normalize 层做补齐。

## 3.3 新增 Reporter 插件

适用场景：输出到 SARIF、CI 注解、平台 API。

要求：
- 不改变核心建议语义。
- 输出应可回溯到 `run_id`、规则 ID 与源码位置。

## 3.4 新增 Transformer 插件

适用场景：补丁生成策略升级。

建议：
- 保持“可审阅”原则。
- 对高风险改写只给建议，不直接生成可应用补丁。

## 3.5 新增 Advisor（AI）插件

适用场景：增强建议解释、验证步骤、风险提示。

建议输入：
- `Suggestions`
- `Semantics`
- 代码上下文

建议输出：
- `explanation`（可执行解释）
- `expected_gain`（收益预估）
- `risk_level`（风险等级）
- `validation_plan`（验证计划）

## 4. 新增 Stage（谨慎）

仅在确有必要时新增阶段。优先复用现有阶段并通过插件增强能力。

新增步骤：
1. 在 `include/optix/stages/` 定义阶段类并实现 `Stage` 接口。
2. 在 `lib/optix/stages/` 实现 `run()`。
3. 在 `tools/optix/main.cpp` 的 pipeline 编排中注册阶段及依赖。
4. 更新设计文档与测试。

## 5. 数据契约与版本策略

## 5.1 元数据约定

所有中间产物必须携带：
- `schema_version`
- `producer`
- `run_id`
- `timestamp`

## 5.2 版本策略

- `v1.x`：只允许新增字段，不删字段。
- 破坏性变更进入 `v2`，并提供适配层。

## 6. 测试开发规范

## 6.1 最低测试要求

- 新规则：单测 + 正反例。
- 新插件：ABI 校验 + 行为测试。
- 新阶段：契约测试 + 端到端测试。

## 6.2 端到端测试建议

至少覆盖：
- `CPU` 瓶颈样例
- `Memory` 瓶颈样例
- `IO` 瓶颈样例
- `Network` 瓶颈样例

并验证：
- 流水线运行成功
- 关键建议命中
- `patch.diff` 可检查
- 报告格式稳定

## 6.3 回归检查清单

每次改动后建议执行：
1. `./build.sh --debug`
2. `ctest --test-dir build --output-on-failure`
3. 抽样检查 `report.json` 与 `events.jsonl`
4. 对核心规则做快照对比

## 7. 常见问题与排障

## 7.1 找不到 ClangConfig.cmake

现象：CMake 配置阶段报错找不到 Clang 包。  
处理：
- 检查 LibTooling 开发包是否安装。
- 显式传入 `Clang_DIR`（指向 `.../lib/cmake/clang`）。

## 7.2 链接时报 LLVM/clang 符号缺失

现象：链接阶段出现 `llvm::*` 未定义符号。  
处理：
- 确认 CMake 已链接 `clang-cpp` 或 `clangTooling*` 目标。
- 确认 `LLVM` 目标已链接。

## 7.3 没有 compile_commands.json

现象：无法提供完整编译数据库。  
处理：
- 当前实现允许缺失内容，但 CLI 参数仍需提供路径。
- 缺失时会使用默认参数并在单文件失败时走文本兜底。

## 8. 提交与文档要求

改动源码后必须同步：
1. 注释（重点类/函数行为）
2. 设计文档（架构或流程有变更时）
3. 开发文档（扩展方式或约束有变化时）
4. 测试（新增能力必须有测试覆盖）

## 9. 推荐开发流程

1. 明确变更边界与风险等级。
2. 先加测试（或先写样例）再实现。
3. 小步提交，优先保证可编译可回归。
4. 用 `run_id` 回放核对行为一致性。
5. 通过后再推进下一批规则/功能。
