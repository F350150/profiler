// diagnose_stage.cpp 实现语义归因阶段。
#include "optix/stages/diagnose_stage.hpp"

#include <stdexcept>

#include "optix/core/metadata.hpp"

namespace optix::stages {
namespace {

// semantic_for 根据指标域映射瓶颈语义标签。
std::string semantic_for(const std::string& domain) {
  if (domain == "cpu") return "CPU_COMPUTE_BOUND";
  if (domain == "memory") return "MEMORY_LOCALITY_POOR";
  if (domain == "io") return "IO_QUEUE_SATURATED";
  if (domain == "network") return "NET_PACKET_OVERHEAD";
  return "UNKNOWN";
}

} // namespace

// run 执行语义归因并输出 Semantics。
optix::core::Artifact DiagnoseStage::run(const optix::core::ArtifactMap& deps,
                                         optix::core::Context& ctx) {
  const auto it = deps.find("normalize");
  if (it == deps.end()) {
    throw std::runtime_error("diagnose requires normalize artifact");
  }
  const auto* sigs = std::get_if<optix::core::Signals>(&it->second.data);
  if (!sigs) {
    throw std::runtime_error("diagnose got invalid artifact type");
  }

  optix::core::Semantics out_rows;
  int seq = 0;
  for (const auto& s : *sigs) {
    if (s.severity == "low") {
      continue;
    }
    optix::core::BottleneckSemantic sem;
    sem.meta = optix::core::make_metadata("stage.diagnose", ctx.run_id);
    sem.id = "sem-" + std::to_string(seq++);
    sem.domain = s.domain;
    sem.semantic = semantic_for(s.domain);
    sem.severity = s.severity;
    sem.confidence = s.confidence;
    sem.evidence_metrics = {s.metric};
    sem.reason = "derived from normalized signal severity and domain policy";
    out_rows.push_back(std::move(sem));
  }

  optix::core::Artifact out;
  out.stage_id = name();
  out.meta = optix::core::make_metadata("stage.diagnose", ctx.run_id);
  out.data = std::move(out_rows);
  return out;
}

} // namespace optix::stages
