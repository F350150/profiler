// normalize_stage.hpp 定义指标标准化阶段。
#pragma once

#include "optix/stages/stage.hpp"

namespace optix::stages {

// NormalizeStage 负责将原始指标转换为统一 Signal。
class NormalizeStage : public Stage {
 public:
  std::string name() const override { return "normalize"; }
  std::string input_schema() const override { return "RawMetricRecords@v1.0"; }
  std::string output_schema() const override { return "Signals@v1.0"; }
  optix::core::Artifact run(const optix::core::ArtifactMap& deps,
                            optix::core::Context& ctx) override;
};

} // namespace optix::stages
