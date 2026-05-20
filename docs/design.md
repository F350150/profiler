# Optix 设计文档

## 1. 背景与目标

`Optix` 的目标是把离散、海量且异构的性能指标（以 `ksys` 为首要输入）统一为稳定性能语义，再将语义映射到源码静态模式，输出可审阅的优化建议和补丁草案。

核心要求：
- 语义优先，不直接耦合某个单指标。
- 配置驱动，规则可版本化、可扩展。
- 结果可追溯、可回放、可审计。
- 首版偏保守：建议优先，自动改写谨慎。

## 2. 设计原则

- 契约稳定：中间产物有统一 schema 与元数据。
- 分层解耦：指标处理、语义归因、代码分析、规则决策、输出生成分离。
- 插件扩展：新增 parser/rulepack/reporter/transformer/advisor 不改内核。
- 可观测优先：所有关键动作可追踪。
- 失败可控：每个阶段有失败策略与降级路径。

## 3. 总体架构

主流水线：
`ingest -> normalize -> diagnose -> index -> rule -> suggest`

横切能力：
- Governance：规则版本、冲突策略、发布节奏。
- Observability/Replay：事件流与产物回放。

### 3.1 分层职责

1. `ingest` 指标接入层  
职责：解析 `ksys` 原始输出并形成标准原始记录。

2. `normalize` 信号标准化层  
职责：单位归一、基线比较、严重度与置信度计算，生成 `Signal`。

3. `diagnose` 语义归因层  
职责：把多指标组合映射为 `BottleneckSemantic`（CPU/Memory/IO/Network）。

4. `index` 静态代码索引层  
职责：从源码提取 `CodeFactGraph`。当前实现为 LibTooling AST 优先，文本兜底。

5. `rule` 决策层  
职责：匹配“语义 + 代码模式”，产出 `CodeOpportunity` 并打分排序。

6. `suggest` 输出层  
职责：生成 `OptimizationSuggestion`、`report.md`、`report.json`、`patch.diff`。

## 4. 核心数据模型

### 4.1 Signal（标准信号）

关键字段：
- `id`
- `domain`
- `metric`
- `scope`
- `window`
- `value`
- `baseline`
- `severity`
- `confidence`

### 4.2 BottleneckSemantic（瓶颈语义）

关键字段：
- `semantic_id`
- `domain`
- `severity`
- `confidence`
- `evidence_metrics[]`
- `attribution_score`

### 4.3 CodeFactGraph（静态事实图）

关键节点：
- `FunctionFact`
- `LoopFact`
- `MemoryAccessFact`
- `IOCallFact`
- `NetCallFact`
- `LockFact`

### 4.4 Opportunity 与 Suggestion

- `CodeOpportunity`：规则命中后候选机会，包含收益/风险/置信度评分。
- `OptimizationSuggestion`：最终输出，包含标题、解释、定位、建议动作与补丁片段。

## 5. Index 设计（LibTooling 严格模式）

### 5.1 关键约束

- 不执行 `clang++`、`ast-dump` 等外部编译器命令。
- 使用 `LibTooling/libclang` 内嵌 AST Matcher。
- CMake 严格要求 LibTooling 可用，否则配置失败。

### 5.2 解析策略

- 优先路径：AST Matcher 提取函数、循环、内存调用、IO 调用、网络调用、锁相关构造。
- 降级路径：单文件 AST 解析失败时，进行文本扫描兜底，保障流程可运行。
- `compile_commands.json`：内容可选，参数入口保留，用于增强编译参数还原精度。

## 6. 编排与失败策略

编排器 `PipelineOrchestrator` 提供：
- DAG 阶段依赖管理
- 阶段级失败策略：
  - `hard_fail`：立即终止
  - `soft_fail`：记录失败后继续
  - `skip`：跳过并继续
- 产物持久化：`artifacts/<run_id>/<stage>.json`

## 7. 插件架构与 ABI

插件固定导出符号：
- `optix_plugin_manifest()`
- `optix_plugin_create()`
- `optix_plugin_destroy()`

`manifest` 关键字段：
- `id`
- `version`
- `engine_api`
- `type`
- `domains`
- `language_targets`
- `min_core_version`

支持插件类型：
- `parser`
- `rulepack`
- `reporter`
- `transformer`
- `advisor`

## 8. 规则系统设计

规则执行顺序：
1. 语义筛选
2. 代码模式匹配
3. 评分计算
4. 冲突消解
5. 建议输出

冲突策略（固定）：
- 同一代码片段保留综合分最高建议。
- 高风险建议可降级为“仅报告不出补丁”。
- 互斥组按 `priority > confidence > benefit` 决策。

## 9. 可观测性与回放

### 9.1 事件流

事件文件：`events.jsonl`  
典型事件：
- `stage_start`
- `stage_end`
- `rule_hit`
- `rule_skip`
- `conflict_resolved`
- `error`

### 9.2 回放

通过 `run_id` 查看阶段产物，复盘诊断链路与规则命中结果，支持问题复现和规则调优。

## 10. 安全与治理

- 默认保守规则集。
- 高风险补丁默认关闭。
- 输出以“建议 + 可审阅补丁”为主，不自动提交代码。
- 规则更新建议采用灰度发布和回归校验。

## 11. 测试策略

- 单元测试：归一化、归因、规则评分、冲突消解。
- 契约测试：Schema 与插件 ABI 兼容。
- 端到端测试：从样例指标到建议输出全链路。
- 稳定性测试：同输入多次运行，建议排序一致。

## 12. 演进路线

- v1.x：字段增量演进，不做破坏性变更。
- v2：如需破坏性调整，提供适配层和迁移脚本。
- AI 扩展：通过 `advisor` 插件接入，不侵入主内核。

## 13. 图示文件

- 类图：`docs/puml/class_diagram.puml`
- 时序图：`docs/puml/sequence_e2e.puml`
- 流程图：`docs/puml/flow_pipeline.puml`
