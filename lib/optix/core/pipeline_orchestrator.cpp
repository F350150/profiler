// pipeline_orchestrator.cpp 实现 Stage DAG 编排执行逻辑。
#include "optix/core/pipeline_orchestrator.hpp"

#include <filesystem>
#include <exception>

#include "optix/core/artifact_io.hpp"

namespace optix::core {

// add_stage 向编排器注册一个阶段。
void PipelineOrchestrator::add_stage(stages::StageSpec spec) {
  stages_.push_back(std::move(spec));
}

// execute 按依赖顺序执行所有阶段并落盘中间产物。
bool PipelineOrchestrator::execute(Context& ctx, ArtifactMap* out_artifacts,
                                   std::string* error) {
  ArtifactMap produced;

  for (const auto& spec : stages_) {
    if (!spec.stage) {
      if (error) *error = "stage pointer is null: " + spec.id;
      return false;
    }

    ArtifactMap deps;
    bool missing_dep = false;
    for (const auto& dep : spec.deps) {
      auto it = produced.find(dep);
      if (it == produced.end()) {
        missing_dep = true;
        if (error) *error = "missing dependency artifact: " + dep + " for stage " + spec.id;
        break;
      }
      deps.emplace(dep, it->second);
    }

    if (missing_dep) {
      if (spec.failure_policy == stages::FailurePolicy::skip) {
        if (ctx.event_bus) {
          ctx.event_bus->publish(EventType::error, spec.id,
                                 "dependency missing; skipped by policy");
        }
        continue;
      }
      if (spec.failure_policy == stages::FailurePolicy::soft_fail) {
        if (ctx.event_bus) {
          ctx.event_bus->publish(EventType::error, spec.id,
                                 "dependency missing; soft fail");
        }
        continue;
      }
      return false;
    }

    if (ctx.event_bus) {
      ctx.event_bus->publish(EventType::stage_start, spec.id, "started");
    }

    try {
      Artifact out = spec.stage->run(deps, ctx);
      produced[spec.id] = out;

      std::filesystem::create_directories(ctx.inputs.output_dir / "artifacts" /
                                          ctx.run_id);
      std::string io_error;
      const auto out_path = ctx.inputs.output_dir / "artifacts" / ctx.run_id /
                            (spec.id + ".json");
      if (!write_artifact_json(out, out_path, &io_error) && ctx.event_bus) {
        ctx.event_bus->publish(EventType::error, spec.id,
                               "artifact write failed: " + io_error);
      }

      if (ctx.event_bus) {
        ctx.event_bus->publish(EventType::stage_end, spec.id, "completed");
      }
    } catch (const std::exception& ex) {
      if (ctx.event_bus) {
        ctx.event_bus->publish(EventType::error, spec.id, ex.what());
      }
      if (spec.failure_policy == stages::FailurePolicy::hard_fail) {
        if (error) *error = "stage failed: " + spec.id + ": " + ex.what();
        return false;
      }
    }
  }

  if (out_artifacts) {
    *out_artifacts = std::move(produced);
  }
  return true;
}

} // namespace optix::core
