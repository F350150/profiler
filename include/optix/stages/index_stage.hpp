// index_stage.hpp 定义静态代码索引阶段。
#pragma once

#include "optix/stages/stage.hpp"

namespace optix::stages {

// IndexStage 负责从源码中提取代码事实图（优先 Clang，失败时兜底文本扫描）。
class IndexStage : public Stage {
 public:
  std::string name() const override { return "index"; }
  std::string input_schema() const override { return "none"; }
  std::string output_schema() const override { return "CodeFactGraph@v1.0"; }
  optix::core::Artifact run(const optix::core::ArtifactMap& deps,
                            optix::core::Context& ctx) override;
};

} // namespace optix::stages
