// test_pipeline.cpp 验证端到端流水线最小可用链路。
#include <cassert>
#include <filesystem>
#include <iostream>

#include "optix/core/context.hpp"
#include "optix/core/event_bus.hpp"
#include "optix/core/metadata.hpp"
#include "optix/core/pipeline_orchestrator.hpp"
#include "optix/stages/diagnose_stage.hpp"
#include "optix/stages/index_stage.hpp"
#include "optix/stages/ingest_stage.hpp"
#include "optix/stages/normalize_stage.hpp"
#include "optix/stages/rule_stage.hpp"
#include "optix/stages/suggest_stage.hpp"

namespace {

// run_pipeline_once 执行一轮流水线并校验基础输出。
void run_pipeline_once(const std::filesystem::path& root,
                       const std::filesystem::path& out,
                       const std::filesystem::path& compile_commands) {
  optix::core::Context ctx;
  ctx.run_id = optix::core::make_run_id();
  ctx.inputs.ksys_path = root / "ksys_sample.log";
  ctx.inputs.source_root = root / "src";
  ctx.inputs.compile_commands = compile_commands;
  ctx.inputs.output_dir = out;

  optix::core::EventBus bus(out / "events.jsonl");
  ctx.event_bus = &bus;

  optix::stages::IngestStage ingest;
  optix::stages::NormalizeStage normalize;
  optix::stages::DiagnoseStage diagnose;
  optix::stages::IndexStage index;
  optix::stages::RuleStage rule;
  optix::stages::SuggestStage suggest;

  optix::core::PipelineOrchestrator orchestrator;
  orchestrator.add_stage({"ingest", {}, optix::stages::FailurePolicy::hard_fail, &ingest});
  orchestrator.add_stage({"normalize", {"ingest"}, optix::stages::FailurePolicy::hard_fail, &normalize});
  orchestrator.add_stage({"diagnose", {"normalize"}, optix::stages::FailurePolicy::hard_fail, &diagnose});
  orchestrator.add_stage({"index", {}, optix::stages::FailurePolicy::hard_fail, &index});
  orchestrator.add_stage({"rule", {"diagnose", "index"}, optix::stages::FailurePolicy::hard_fail, &rule});
  orchestrator.add_stage({"suggest", {"rule"}, optix::stages::FailurePolicy::hard_fail, &suggest});

  optix::core::ArtifactMap artifacts;
  std::string error;
  const bool ok = orchestrator.execute(ctx, &artifacts, &error);
  if (!ok) {
    std::cerr << error << "\n";
  }
  assert(ok);
  assert(artifacts.find("suggest") != artifacts.end());
  assert(artifacts.find("index") != artifacts.end());
  assert(std::filesystem::exists(out / "report.md"));
  assert(std::filesystem::exists(out / "report.json"));
  assert(std::filesystem::exists(out / "patch.diff"));

  const auto* graph = std::get_if<optix::core::CodeFactGraph>(&artifacts["index"].data);
  assert(graph != nullptr);
  assert(!graph->functions.empty());
}

} // namespace

// main 执行基础集成测试并校验关键输出文件。
int main() {
  namespace fs = std::filesystem;
  const fs::path root = fs::path(__FILE__).parent_path() / "data";

  // Case 1: 提供 compile_commands.json。
  const fs::path out_with_cc = fs::temp_directory_path() / "optix-test-out-with-cc";
  fs::create_directories(out_with_cc);
  run_pipeline_once(root, out_with_cc, root / "compile_commands.json");

  // Case 2: compile_commands.json 缺失时也必须可运行（走 LibTooling/文本兜底路径）。
  const fs::path out_without_cc = fs::temp_directory_path() / "optix-test-out-no-cc";
  fs::create_directories(out_without_cc);
  run_pipeline_once(root, out_without_cc, root / "missing_compile_commands.json");

  std::cout << "optix pipeline test passed\n";
  return 0;
}
