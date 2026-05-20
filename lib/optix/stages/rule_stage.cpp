// rule_stage.cpp 实现规则匹配与机会点生成逻辑。
#include "optix/stages/rule_stage.hpp"

#include <algorithm>
#include <set>
#include <stdexcept>

#include "optix/core/metadata.hpp"
#include "optix/plugins/api.hpp"

namespace optix::stages {
namespace {

// fallback_match 是无 rulepack 插件时的兜底规则匹配。
optix::core::Opportunities fallback_match(const optix::core::Semantics& semantics,
                                          const optix::core::CodeFactGraph& graph,
                                          const std::string& run_id) {
  optix::core::Opportunities out;
  int seq = 0;

  for (const auto& sem : semantics) {
    if (sem.semantic == "CPU_COMPUTE_BOUND") {
      for (const auto& loop : graph.loops) {
        if (!loop.has_contiguous_access) continue;
        optix::core::CodeOpportunity o;
        o.meta = optix::core::make_metadata("rulepack.fallback", run_id);
        o.id = "opp-" + std::to_string(seq++);
        o.semantic_id = sem.id;
        o.file = loop.file;
        o.function = loop.function;
        o.line = loop.line;
        o.pattern = "vectorizable_loop";
        o.title = "CPU cycle hotspot on vectorizable loop";
        o.recommendation = "consider SIMD pragma/intrinsics and hoist invariants";
        o.benefit_score = 0.85;
        o.risk_score = 0.30;
        o.confidence_score = sem.confidence;
        o.exclusive_group = "cpu_loop_opt";
        o.priority = 100;
        out.push_back(std::move(o));
      }
    } else if (sem.semantic == "MEMORY_LOCALITY_POOR") {
      for (const auto& mem : graph.memory_accesses) {
        optix::core::CodeOpportunity o;
        o.meta = optix::core::make_metadata("rulepack.fallback", run_id);
        o.id = "opp-" + std::to_string(seq++);
        o.semantic_id = sem.id;
        o.file = mem.file;
        o.function = mem.function;
        o.line = mem.line;
        o.pattern = "alloc_or_copy_hotpath";
        o.title = "Memory locality bottleneck candidate";
        o.recommendation = "reduce transient allocations and improve data locality";
        o.benefit_score = 0.75;
        o.risk_score = 0.35;
        o.confidence_score = sem.confidence;
        o.exclusive_group = "mem_locality_opt";
        o.priority = 90;
        out.push_back(std::move(o));
      }
    } else if (sem.semantic == "IO_QUEUE_SATURATED") {
      for (const auto& io : graph.io_calls) {
        optix::core::CodeOpportunity o;
        o.meta = optix::core::make_metadata("rulepack.fallback", run_id);
        o.id = "opp-" + std::to_string(seq++);
        o.semantic_id = sem.id;
        o.file = io.file;
        o.function = io.function;
        o.line = io.line;
        o.pattern = "sync_io_hotpath";
        o.title = "Synchronous IO saturation candidate";
        o.recommendation = "batch small I/O and consider async pipeline";
        o.benefit_score = 0.70;
        o.risk_score = 0.25;
        o.confidence_score = sem.confidence;
        o.exclusive_group = "io_sync_opt";
        o.priority = 80;
        out.push_back(std::move(o));
      }
    } else if (sem.semantic == "NET_PACKET_OVERHEAD") {
      for (const auto& net : graph.net_calls) {
        optix::core::CodeOpportunity o;
        o.meta = optix::core::make_metadata("rulepack.fallback", run_id);
        o.id = "opp-" + std::to_string(seq++);
        o.semantic_id = sem.id;
        o.file = net.file;
        o.function = net.function;
        o.line = net.line;
        o.pattern = "small_packet_hotpath";
        o.title = "Network small-packet overhead candidate";
        o.recommendation = "batch send/recv and reduce serialization overhead";
        o.benefit_score = 0.72;
        o.risk_score = 0.28;
        o.confidence_score = sem.confidence;
        o.exclusive_group = "net_pkt_opt";
        o.priority = 85;
        out.push_back(std::move(o));
      }
    }
  }

  std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) {
    if (a.priority != b.priority) return a.priority > b.priority;
    if (a.confidence_score != b.confidence_score) return a.confidence_score > b.confidence_score;
    return a.benefit_score > b.benefit_score;
  });

  // Conflict policy: same file/line + same exclusive group keeps highest rank only.
  optix::core::Opportunities dedup;
  std::set<std::string> seen;
  for (const auto& o : out) {
    const std::string key = o.file + ":" + std::to_string(o.line) + ":" + o.exclusive_group;
    if (seen.insert(key).second) {
      dedup.push_back(o);
    }
  }

  return dedup;
}

} // namespace

// run 执行规则匹配阶段并输出机会点列表。
optix::core::Artifact RuleStage::run(const optix::core::ArtifactMap& deps,
                                     optix::core::Context& ctx) {
  const auto it_sem = deps.find("diagnose");
  const auto it_idx = deps.find("index");
  if (it_sem == deps.end() || it_idx == deps.end()) {
    throw std::runtime_error("rule stage requires diagnose and index artifacts");
  }

  const auto* sems = std::get_if<optix::core::Semantics>(&it_sem->second.data);
  const auto* graph = std::get_if<optix::core::CodeFactGraph>(&it_idx->second.data);
  if (!sems || !graph) {
    throw std::runtime_error("rule stage got invalid artifact types");
  }

  optix::core::Opportunities ops;
  if (ctx.rulepack_plugin) {
    ops = ctx.rulepack_plugin->match(*sems, *graph);
  } else {
    ops = fallback_match(*sems, *graph, ctx.run_id);
  }

  for (const auto& o : ops) {
    if (ctx.event_bus) {
      ctx.event_bus->publish(optix::core::EventType::rule_hit, name(),
                             o.id + " " + o.title);
    }
  }

  optix::core::Artifact out;
  out.stage_id = name();
  out.meta = optix::core::make_metadata("stage.rule", ctx.run_id);
  out.data = std::move(ops);
  return out;
}

} // namespace optix::stages
