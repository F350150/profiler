// types.hpp 定义 optix 全流程核心数据类型。
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace optix::core {

// Metadata 描述所有产物共用的元信息。
struct Metadata {
  std::string schema_version;
  std::string producer;
  std::string run_id;
  std::string timestamp;
};

// RawMetricRecord 表示原始指标记录（尚未归一化）。
struct RawMetricRecord {
  Metadata meta;
  std::string domain;
  std::string metric;
  double value{0.0};
  std::string unit;
  std::string scope;
  std::string window;
};

// Signal 表示归一化后的标准信号。
struct Signal {
  Metadata meta;
  std::string id;
  std::string domain;
  std::string metric;
  std::string scope;
  std::string window;
  double value{0.0};
  double baseline{0.0};
  std::string severity;
  double confidence{0.0};
};

// BottleneckSemantic 表示语义化瓶颈归因结果。
struct BottleneckSemantic {
  Metadata meta;
  std::string id;
  std::string domain;
  std::string semantic;
  std::string severity;
  double confidence{0.0};
  std::vector<std::string> evidence_metrics;
  std::string reason;
};

// FunctionFact 记录函数级静态事实。
struct FunctionFact {
  std::string file;
  std::string function;
  int line{1};
};

// LoopFact 记录循环结构与访问模式信息。
struct LoopFact {
  std::string file;
  std::string function;
  int line{1};
  bool has_contiguous_access{false};
};

// MemoryAccessFact 记录内存访问相关事实。
struct MemoryAccessFact {
  std::string file;
  std::string function;
  int line{1};
  std::string pattern;
};

// IOCallFact 记录 IO 调用事实。
struct IOCallFact {
  std::string file;
  std::string function;
  int line{1};
  std::string call_name;
};

// NetCallFact 记录网络调用事实。
struct NetCallFact {
  std::string file;
  std::string function;
  int line{1};
  std::string call_name;
};

// LockFact 记录锁与并发原语使用事实。
struct LockFact {
  std::string file;
  std::string function;
  int line{1};
  std::string primitive;
};

// CodeFactGraph 汇总静态分析阶段提取的代码事实图。
struct CodeFactGraph {
  Metadata meta;
  std::vector<FunctionFact> functions;
  std::vector<LoopFact> loops;
  std::vector<MemoryAccessFact> memory_accesses;
  std::vector<IOCallFact> io_calls;
  std::vector<NetCallFact> net_calls;
  std::vector<LockFact> locks;
};

// CodeOpportunity 表示规则命中的可优化机会点。
struct CodeOpportunity {
  Metadata meta;
  std::string id;
  std::string semantic_id;
  std::string file;
  std::string function;
  int line{1};
  std::string pattern;
  std::string title;
  std::string recommendation;
  double benefit_score{0.0};
  double risk_score{0.0};
  double confidence_score{0.0};
  std::string exclusive_group;
  int priority{0};
};

// OptimizationSuggestion 表示最终可输出给开发者的优化建议。
struct OptimizationSuggestion {
  Metadata meta;
  std::string id;
  std::string file;
  int line{1};
  std::string title;
  std::string explanation;
  std::string expected_gain;
  std::string risk_level;
  std::string patch;
};

// 常用集合别名。
using RawMetricRecords = std::vector<RawMetricRecord>;
using Signals = std::vector<Signal>;
using Semantics = std::vector<BottleneckSemantic>;
using Opportunities = std::vector<CodeOpportunity>;
using Suggestions = std::vector<OptimizationSuggestion>;

} // namespace optix::core
