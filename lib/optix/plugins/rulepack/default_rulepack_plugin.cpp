// default_rulepack_plugin.cpp 实现默认规则包插件。
#include "optix/core/metadata.hpp"
#include "optix/plugins/api.hpp"

namespace {

// DefaultRulePackPlugin 负责语义到机会点的规则匹配。
class DefaultRulePackPlugin final : public optix::plugins::IRulePackPlugin {
 public:
  // 返回插件清单信息。
  const optix::plugins::PluginManifest& manifest() const override { return manifest_; }

  // 根据语义与代码事实输出机会点。
  optix::core::Opportunities match(const optix::core::Semantics& semantics,
                                   const optix::core::CodeFactGraph& graph) override {
    optix::core::Opportunities out;
    int seq = 0;
    for (const auto& sem : semantics) {
      if (sem.semantic == "CPU_COMPUTE_BOUND") {
        for (const auto& loop : graph.loops) {
          if (!loop.has_contiguous_access) continue;
          out.push_back(make_opportunity(seq++, sem.id, loop.file, loop.function, loop.line,
                                         "vectorizable_loop", "SIMD optimization candidate",
                                         "apply vectorization pragma/intrinsics", 0.85, 0.3,
                                         sem.confidence, "cpu_loop", 100));
        }
      }
      if (sem.semantic == "MEMORY_LOCALITY_POOR") {
        for (const auto& m : graph.memory_accesses) {
          out.push_back(make_opportunity(seq++, sem.id, m.file, m.function, m.line,
                                         "memory_locality", "Memory layout optimization candidate",
                                         "reduce transient allocation and improve locality", 0.75,
                                         0.35, sem.confidence, "mem_layout", 90));
        }
      }
      if (sem.semantic == "IO_QUEUE_SATURATED") {
        for (const auto& io : graph.io_calls) {
          out.push_back(make_opportunity(seq++, sem.id, io.file, io.function, io.line,
                                         "sync_io", "I/O batching candidate",
                                         "batch small sync IO into larger chunks", 0.7, 0.25,
                                         sem.confidence, "io_batch", 85));
        }
      }
      if (sem.semantic == "NET_PACKET_OVERHEAD") {
        for (const auto& net : graph.net_calls) {
          out.push_back(make_opportunity(seq++, sem.id, net.file, net.function, net.line,
                                         "small_packet", "Network batching candidate",
                                         "batch small packets or reduce serialization", 0.72, 0.28,
                                         sem.confidence, "net_batch", 88));
        }
      }
    }
    return out;
  }

 private:
  // make_opportunity 构造统一机会点对象。
  static optix::core::CodeOpportunity make_opportunity(
      int seq, const std::string& sem_id, const std::string& file,
      const std::string& func, int line, const std::string& pattern,
      const std::string& title, const std::string& recommendation, double benefit,
      double risk, double confidence, const std::string& group, int priority) {
    optix::core::CodeOpportunity o;
    o.meta = optix::core::make_metadata("plugin.rulepack", "external");
    o.id = "opp-" + std::to_string(seq);
    o.semantic_id = sem_id;
    o.file = file;
    o.function = func;
    o.line = line;
    o.pattern = pattern;
    o.title = title;
    o.recommendation = recommendation;
    o.benefit_score = benefit;
    o.risk_score = risk;
    o.confidence_score = confidence;
    o.exclusive_group = group;
    o.priority = priority;
    return o;
  }

  optix::plugins::PluginManifest manifest_{
      "default_rulepack", "0.1.0", "v1", optix::plugins::PluginType::rulepack,
      "cpu,memory,io,network", "c,cpp", "0.1.0"};
};

} // namespace

// 导出插件清单符号。
extern "C" const optix::plugins::PluginManifest* optix_plugin_manifest() {
  static DefaultRulePackPlugin plugin;
  return &plugin.manifest();
}

// 导出插件创建符号。
extern "C" void* optix_plugin_create() {
  return new DefaultRulePackPlugin();
}

// 导出插件销毁符号。
extern "C" void optix_plugin_destroy(void* p) {
  delete reinterpret_cast<DefaultRulePackPlugin*>(p);
}
