// suggest_stage.hpp 定义建议与报告生成阶段。
#pragma once

#include "optix/stages/stage.hpp"

namespace optix::stages {

// SuggestStage 负责生成建议、补丁和最终报告。
class SuggestStage : public Stage {
 public:
  std::string name() const override { return "suggest"; }
  std::string input_schema() const override { return "Opportunities@v1.0"; }
  std::string output_schema() const override { return "Suggestions@v1.0"; }
  optix::core::Artifact run(const optix::core::ArtifactMap& deps,
                            optix::core::Context& ctx) override;
};

} // namespace optix::stages
