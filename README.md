# Optix

`Optix` 是一个面向 C/C++ 的性能优化建议引擎：将 `ksys` 指标信号映射为性能语义，再结合静态代码分析给出可审阅的优化建议与补丁草案。

## 1. 项目目标

- 输入：`ksys` 输出、源码目录、编译数据库（可选内容，参数必填）。
- 核心能力：
  - 指标标准化（`ingest/normalize`）
  - 瓶颈语义归因（`diagnose`）
  - C/C++ 静态代码索引（`index`）
  - 规则匹配与冲突消解（`rule`）
  - 建议、报告、补丁输出（`suggest`）
- 输出：`report.md`、`report.json`、`patch.diff`、阶段产物与事件日志。

## 2. 当前特性（已实现）

- 可插拔流水线：`ingest -> normalize -> diagnose -> index -> rule -> suggest`
- 稳定中间产物契约（所有产物含统一元数据）：
  - `schema_version`
  - `producer`
  - `run_id`
  - `timestamp`
- Pipeline 编排器：
  - DAG 执行
  - 阶段失败策略：`hard_fail`、`soft_fail`、`skip`
- 插件机制（C ABI）：
  - `optix_plugin_manifest`
  - `optix_plugin_create`
  - `optix_plugin_destroy`
- 可观测性：
  - 事件流 `events.jsonl`（`stage_start/stage_end/rule_hit/rule_skip/conflict_resolved/error`）
  - 可回放产物目录 `artifacts/<run_id>/`
- 静态分析实现：
  - `LibTooling/libclang` AST Matcher 优先
  - 不调用 `clang++` 二进制命令
  - 单文件 AST 失败时自动回退文本兜底

## 3. 技术栈

- 语言：`C++20`
- 构建：`CMake >= 3.20`
- 关键依赖：
  - `Clang LibTooling/libclang`（严格必需）
  - `nlohmann/json`
  - `yaml-cpp`

## 4. 目录结构

```text
optix/
  include/optix/         # 头文件（对外接口与核心类型）
  lib/optix/             # 核心实现、stage、插件实现
  tools/optix/           # CLI 入口
  docs/                  # 设计与开发文档
  schemas/               # Schema 声明
  rules/                 # 规则 DSL
  tests/                 # 测试与样例数据
  build.sh               # 一键构建脚本
```

## 5. 构建与测试

### 5.1 一键构建（推荐）

```bash
./build.sh --debug
./build.sh --release --test
```

### 5.2 手动构建

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## 6. CLI 使用说明

> 可执行文件默认是 `./build/optix`。

### 6.1 一键全流程

```bash
./build/optix run \
  --ksys tests/data/ksys_sample.log \
  --src tests/data/src \
  --build-db tests/data/compile_commands.json \
  --out /tmp/optix-out
```

说明：
- `--build-db` 参数当前是必填；即便没有真实 `compile_commands.json`，也可以传一个不存在路径，索引阶段会走默认参数+兜底策略。

### 6.2 分阶段执行

```bash
./build/optix ingest --ksys tests/data/ksys_sample.log --out /tmp/ingest.json
./build/optix normalize --input /tmp/ingest.json --out /tmp/normalize.json
./build/optix diagnose --metrics /tmp/normalize.json --out /tmp/semantics.json
./build/optix index --src tests/data/src --build-db tests/data/compile_commands.json --out /tmp/index.json
./build/optix suggest --semantics /tmp/semantics.json --index /tmp/index.json --out /tmp/suggest-out
```

### 6.3 应用建议（实验能力）

```bash
./build/optix apply \
  --suggest /tmp/suggest-out/suggest.json \
  --src-root tests/data/src \
  --out /tmp/apply-out
```

`apply` 当前行为：
- 备份原文件到 `out/backups/`
- 在目标位置插入 `// OPTIX_APPLIED: ...` 注释提示
- 生成 `apply_summary.txt`

### 6.4 插件命令

```bash
./build/optix plugin validate --path ./build/optix_plugins_default_rulepack.dylib
./build/optix plugin list --plugin ./build/optix_plugins_default_rulepack.dylib
./build/optix plugin enable --id default_rulepack
./build/optix plugin disable --id default_rulepack
```

### 6.5 回放阶段产物

```bash
./build/optix replay --run-id <run_id> --out /tmp/optix-out
```

## 7. 输出说明

- `report.md`：面向人工评审的优化建议报告
- `report.json`：面向 CI/平台消费的机器可读结果
- `patch.diff`：可审阅补丁草案
- `events.jsonl`：可观测事件流
- `artifacts/<run_id>/*.json`：阶段中间产物

## 8. 已知约束

- 当前主要面向 Linux 风格源码模式与 C/C++ 项目。
- 静态分析首版关注常见模式：
  - 可向量化循环候选
  - 内存分配/拷贝热点模式
  - 同步 IO 调用路径
  - 小包网络调用路径
- 规则默认偏保守，不会自动提交代码。

## 9. 路线建议（下一步）

- 增加更多语义归因规则（跨域联合诊断）
- 增强 AST 模式匹配精度与上下文分析
- 引入 AI advisor 插件，补充优化解释与验证计划
- 输出 SARIF/CI 注解格式

## 10. 文档索引

- 设计文档：`docs/design.md`
- 开发者文档：`docs/development.md`
- PUML 图：`docs/puml/`
