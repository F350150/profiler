// diagnose_stage.hpp 定义语义归因阶段。
#pragma once

#include "optix/stages/stage.hpp"

namespace optix::stages {

// DiagnoseStage 负责把 Signal 聚合为语义化瓶颈结果。
class DiagnoseStage : public Stage {
 public:
  std::string name() const override { return "diagnose"; }
  std::string input_schema() const override { return "Signals@v1.0"; }
  std::string output_schema() const override { return "Semantics@v1.0"; }
  optix::core::Artifact run(const optix::core::ArtifactMap& deps,
                            optix::core::Context& ctx) override;
};

} // namespace optix::stages
