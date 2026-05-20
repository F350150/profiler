// pipeline_orchestrator.hpp 定义 DAG 流水线编排器接口。
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "optix/core/artifact.hpp"
#include "optix/core/context.hpp"
#include "optix/stages/stage.hpp"

namespace optix::core {

// PipelineOrchestrator 负责按依赖顺序执行 Stage，并落盘中间产物。
class PipelineOrchestrator {
 public:
  // 注册一个阶段定义。
  void add_stage(stages::StageSpec spec);
  // 执行全部阶段，返回是否成功。
  bool execute(Context& ctx, ArtifactMap* out_artifacts, std::string* error = nullptr);

 private:
  std::vector<stages::StageSpec> stages_;
};

} // namespace optix::core
