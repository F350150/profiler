// normalize_stage.cpp 实现指标标准化阶段。
#include "optix/stages/normalize_stage.hpp"

#include <stdexcept>

#include "optix/core/metadata.hpp"

namespace optix::stages {
namespace {

// severity_from_value 根据数值区间计算严重等级。
std::string severity_from_value(double v) {
  if (v >= 0.8) return "high";
  if (v >= 0.6) return "medium";
  return "low";
}

// confidence_from_value 根据数值区间计算置信度。
double confidence_from_value(double v) {
  if (v >= 0.8) return 0.9;
  if (v >= 0.6) return 0.75;
  return 0.55;
}

} // namespace

// run 将 RawMetricRecords 转换为标准 Signal 列表。
optix::core::Artifact NormalizeStage::run(const optix::core::ArtifactMap& deps,
                                          optix::core::Context& ctx) {
  const auto it = deps.find("ingest");
  if (it == deps.end()) {
    throw std::runtime_error("normalize requires ingest artifact");
  }
  const auto* rows = std::get_if<optix::core::RawMetricRecords>(&it->second.data);
  if (!rows) {
    throw std::runtime_error("normalize got invalid artifact type");
  }

  optix::core::Signals signals;
  int seq = 0;
  for (const auto& r : *rows) {
    optix::core::Signal s;
    s.meta = optix::core::make_metadata("stage.normalize", ctx.run_id);
    s.id = "sig-" + std::to_string(seq++);
    s.domain = r.domain;
    s.metric = r.metric;
    s.scope = r.scope;
    s.window = r.window;
    s.value = r.value;
    s.baseline = 0.5;
    s.severity = severity_from_value(r.value);
    s.confidence = confidence_from_value(r.value);
    signals.push_back(std::move(s));
  }

  optix::core::Artifact out;
  out.stage_id = name();
  out.meta = optix::core::make_metadata("stage.normalize", ctx.run_id);
  out.data = std::move(signals);
  return out;
}

} // namespace optix::stages
