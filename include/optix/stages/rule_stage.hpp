// rule_stage.hpp 定义规则匹配阶段。
#pragma once

#include "optix/stages/stage.hpp"

namespace optix::stages {

// RuleStage 负责把语义瓶颈映射为代码优化机会点。
class RuleStage : public Stage {
 public:
  std::string name() const override { return "rule"; }
  std::string input_schema() const override { return "Semantics@v1.0 + CodeFactGraph@v1.0"; }
  std::string output_schema() const override { return "Opportunities@v1.0"; }
  optix::core::Artifact run(const optix::core::ArtifactMap& deps,
                            optix::core::Context& ctx) override;
};

} // namespace optix::stages
