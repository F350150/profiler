// main.cpp 是 optix 命令行工具入口，负责参数解析与子命令分发。
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "optix/core/artifact_io.hpp"
#include "optix/core/context.hpp"
#include "optix/core/event_bus.hpp"
#include "optix/core/metadata.hpp"
#include "optix/core/pipeline_orchestrator.hpp"
#include "optix/plugins/plugin_manager.hpp"
#include "optix/stages/diagnose_stage.hpp"
#include "optix/stages/index_stage.hpp"
#include "optix/stages/ingest_stage.hpp"
#include "optix/stages/normalize_stage.hpp"
#include "optix/stages/rule_stage.hpp"
#include "optix/stages/suggest_stage.hpp"

namespace {

// parse_flags 解析形如 `--key value` 的命令行参数。
std::unordered_map<std::string, std::string> parse_flags(int argc, char** argv,
                                                         int start_index) {
  std::unordered_map<std::string, std::string> flags;
  for (int i = start_index; i < argc; ++i) {
    std::string token = argv[i];
    if (token.rfind("--", 0) == 0 && i + 1 < argc) {
      flags[token.substr(2)] = argv[++i];
    }
  }
  return flags;
}

// split_csv 将逗号分隔字符串拆分为列表。
std::vector<std::string> split_csv(const std::string& in) {
  std::vector<std::string> out;
  std::string cur;
  std::stringstream ss(in);
  while (std::getline(ss, cur, ',')) {
    if (!cur.empty()) out.push_back(cur);
  }
  return out;
}

// print_help 打印命令行帮助信息。
void print_help() {
  std::cout << "optix commands:\n"
            << "  run --ksys <path> --src <path> --build-db <path> --out <dir> [--plugin <so>] [--plugins <a.so,b.so>]\n"
            << "  ingest --ksys <path> --out <artifact.json>\n"
            << "  normalize --input <artifact.json> --out <artifact.json>\n"
            << "  index --src <path> --build-db <path> --out <artifact.json>\n"
            << "  diagnose --metrics <artifact.json> --out <artifact.json>\n"
            << "  suggest --semantics <artifact.json> --index <artifact.json> --out <dir>\n"
            << "  apply --suggest <artifact.json> --src-root <path> --out <dir>\n"
            << "  plugin list [--plugin <so>]\n"
            << "  plugin validate --path <plugin.so>\n"
            << "  plugin enable --id <plugin_id> [--config <file>]\n"
            << "  plugin disable --id <plugin_id> [--config <file>]\n"
            << "  replay --run-id <id> --out <dir>\n";
}

// load_optional_plugin 按单个 flag 尝试加载插件。
bool load_optional_plugin(optix::plugins::PluginManager& pm,
                          const std::unordered_map<std::string, std::string>& flags,
                          const char* flag_name) {
  auto it = flags.find(flag_name);
  if (it == flags.end()) return true;
  std::string err;
  if (!pm.load(it->second, &err)) {
    std::cerr << "failed to load plugin: " << err << "\n";
    return false;
  }
  return true;
}

// load_plugins 统一处理单插件与多插件加载。
bool load_plugins(optix::plugins::PluginManager& pm,
                  const std::unordered_map<std::string, std::string>& flags) {
  if (!load_optional_plugin(pm, flags, "plugin")) {
    return false;
  }
  auto it = flags.find("plugins");
  if (it == flags.end()) {
    return true;
  }
  for (const auto& path : split_csv(it->second)) {
    std::string err;
    if (!pm.load(path, &err)) {
      std::cerr << "failed to load plugin: " << path << " error: " << err << "\n";
      return false;
    }
  }
  return true;
}

// run_pipeline 执行完整端到端流水线。
int run_pipeline(const std::unordered_map<std::string, std::string>& flags) {
  const auto ksys = flags.find("ksys");
  const auto src = flags.find("src");
  const auto build_db = flags.find("build-db");
  const auto out = flags.find("out");
  if (ksys == flags.end() || src == flags.end() || build_db == flags.end() ||
      out == flags.end()) {
    std::cerr << "missing required flags for run\n";
    return 1;
  }

  optix::plugins::PluginManager pm;
  if (!load_plugins(pm, flags)) {
    return 3;
  }

  const std::string run_id = optix::core::make_run_id();
  std::filesystem::create_directories(out->second);
  optix::core::EventBus event_bus(std::filesystem::path(out->second) / "events.jsonl");

  optix::core::Context ctx;
  ctx.run_id = run_id;
  ctx.inputs.ksys_path = ksys->second;
  ctx.inputs.source_root = src->second;
  ctx.inputs.compile_commands = build_db->second;
  ctx.inputs.output_dir = out->second;
  ctx.event_bus = &event_bus;
  ctx.plugin_manager = &pm;
  ctx.parser_plugin = pm.parser();
  ctx.rulepack_plugin = pm.rulepack();
  ctx.reporter_plugin = pm.reporter();
  ctx.transformer_plugin = pm.transformer();
  ctx.advisor_plugin = pm.advisor();

  optix::stages::IngestStage ingest;
  optix::stages::NormalizeStage normalize;
  optix::stages::DiagnoseStage diagnose;
  optix::stages::IndexStage index;
  optix::stages::RuleStage rule;
  optix::stages::SuggestStage suggest;

  optix::core::PipelineOrchestrator orchestrator;
  orchestrator.add_stage({"ingest", {}, optix::stages::FailurePolicy::hard_fail, &ingest});
  orchestrator.add_stage({"normalize", {"ingest"}, optix::stages::FailurePolicy::hard_fail,
                          &normalize});
  orchestrator.add_stage({"diagnose", {"normalize"},
                          optix::stages::FailurePolicy::hard_fail, &diagnose});
  orchestrator.add_stage({"index", {}, optix::stages::FailurePolicy::hard_fail, &index});
  orchestrator.add_stage({"rule", {"diagnose", "index"},
                          optix::stages::FailurePolicy::hard_fail, &rule});
  orchestrator.add_stage({"suggest", {"rule"}, optix::stages::FailurePolicy::hard_fail,
                          &suggest});

  optix::core::ArtifactMap outputs;
  std::string err;
  if (!orchestrator.execute(ctx, &outputs, &err)) {
    std::cerr << "pipeline failed: " << err << "\n";
    return 2;
  }

  std::cout << "run_id=" << run_id << "\n";
  std::cout << "outputs in: " << out->second << "\n";
  return 0;
}

// run_ingest 单独执行 ingest 阶段。
int run_ingest(const std::unordered_map<std::string, std::string>& flags) {
  const auto ksys = flags.find("ksys");
  const auto out = flags.find("out");
  if (ksys == flags.end() || out == flags.end()) {
    std::cerr << "missing required flags for ingest\n";
    return 1;
  }

  optix::core::Context ctx;
  ctx.run_id = optix::core::make_run_id();
  ctx.inputs.ksys_path = ksys->second;

  optix::stages::IngestStage stage;
  auto artifact = stage.run({}, ctx);
  std::string err;
  if (!optix::core::write_artifact_json(artifact, out->second, &err)) {
    std::cerr << err << "\n";
    return 2;
  }
  return 0;
}

// run_index 单独执行 index 阶段。
int run_index(const std::unordered_map<std::string, std::string>& flags) {
  const auto src = flags.find("src");
  const auto out = flags.find("out");
  const auto build_db = flags.find("build-db");
  if (src == flags.end() || out == flags.end() || build_db == flags.end()) {
    std::cerr << "missing required flags for index\n";
    return 1;
  }

  optix::core::Context ctx;
  ctx.run_id = optix::core::make_run_id();
  ctx.inputs.source_root = src->second;
  ctx.inputs.compile_commands = build_db->second;

  optix::stages::IndexStage stage;
  auto artifact = stage.run({}, ctx);
  std::string err;
  if (!optix::core::write_artifact_json(artifact, out->second, &err)) {
    std::cerr << err << "\n";
    return 2;
  }
  return 0;
}

// run_normalize 单独执行 normalize 阶段。
int run_normalize(const std::unordered_map<std::string, std::string>& flags) {
  const auto input = flags.find("input");
  const auto out = flags.find("out");
  if (input == flags.end() || out == flags.end()) {
    std::cerr << "missing required flags for normalize\n";
    return 1;
  }

  optix::core::Artifact in;
  std::string err;
  if (!optix::core::read_artifact_json(input->second, &in, &err)) {
    std::cerr << err << "\n";
    return 2;
  }

  optix::core::Context ctx;
  ctx.run_id = optix::core::make_run_id();
  optix::stages::NormalizeStage stage;
  optix::core::ArtifactMap deps;
  deps["ingest"] = in;
  auto artifact = stage.run(deps, ctx);
  if (!optix::core::write_artifact_json(artifact, out->second, &err)) {
    std::cerr << err << "\n";
    return 3;
  }
  return 0;
}

// run_diagnose 单独执行 diagnose 阶段。
int run_diagnose(const std::unordered_map<std::string, std::string>& flags) {
  const auto metrics = flags.find("metrics");
  const auto out = flags.find("out");
  if (metrics == flags.end() || out == flags.end()) {
    std::cerr << "missing required flags for diagnose\n";
    return 1;
  }

  optix::core::Artifact in;
  std::string err;
  if (!optix::core::read_artifact_json(metrics->second, &in, &err)) {
    std::cerr << err << "\n";
    return 2;
  }

  optix::core::Context ctx;
  ctx.run_id = optix::core::make_run_id();

  optix::stages::DiagnoseStage stage;
  optix::core::ArtifactMap deps;
  deps["normalize"] = in;
  auto artifact = stage.run(deps, ctx);
  if (!optix::core::write_artifact_json(artifact, out->second, &err)) {
    std::cerr << err << "\n";
    return 3;
  }
  return 0;
}

// run_suggest 单独执行 rule + suggest 阶段。
int run_suggest(const std::unordered_map<std::string, std::string>& flags) {
  const auto semantics = flags.find("semantics");
  const auto index = flags.find("index");
  const auto out = flags.find("out");
  if (semantics == flags.end() || index == flags.end() || out == flags.end()) {
    std::cerr << "missing required flags for suggest\n";
    return 1;
  }

  optix::core::Artifact sem_art;
  optix::core::Artifact idx_art;
  std::string err;
  if (!optix::core::read_artifact_json(semantics->second, &sem_art, &err) ||
      !optix::core::read_artifact_json(index->second, &idx_art, &err)) {
    std::cerr << err << "\n";
    return 2;
  }

  optix::core::Context ctx;
  ctx.run_id = optix::core::make_run_id();
  ctx.inputs.output_dir = out->second;

  optix::stages::RuleStage rule_stage;
  optix::stages::SuggestStage suggest_stage;
  optix::core::ArtifactMap deps_rule;
  deps_rule["diagnose"] = sem_art;
  deps_rule["index"] = idx_art;
  auto rule_art = rule_stage.run(deps_rule, ctx);

  optix::core::ArtifactMap deps_suggest;
  deps_suggest["rule"] = rule_art;
  auto suggest_art = suggest_stage.run(deps_suggest, ctx);
  if (!optix::core::write_artifact_json(suggest_art,
                                        std::filesystem::path(out->second) / "suggest.json",
                                        &err)) {
    std::cerr << err << "\n";
    return 3;
  }
  return 0;
}

// resolve_target_file 根据建议中的路径定位源码文件。
std::filesystem::path resolve_target_file(const std::filesystem::path& src_root,
                                          const std::string& suggestion_file) {
  std::filesystem::path p(suggestion_file);
  if (p.is_absolute() && std::filesystem::exists(p)) {
    return p;
  }
  auto joined = src_root / p;
  if (std::filesystem::exists(joined)) {
    return joined;
  }
  if (p.has_filename()) {
    const auto fname = p.filename();
    for (auto it = std::filesystem::recursive_directory_iterator(src_root);
         it != std::filesystem::recursive_directory_iterator(); ++it) {
      if (!it->is_regular_file()) continue;
      if (it->path().filename() == fname) {
        return it->path();
      }
    }
  }
  return joined;
}

// run_apply 将建议注入源码并生成备份与摘要。
int run_apply(const std::unordered_map<std::string, std::string>& flags) {
  const auto suggest = flags.find("suggest");
  const auto src_root = flags.find("src-root");
  const auto out = flags.find("out");
  if (suggest == flags.end() || src_root == flags.end() || out == flags.end()) {
    std::cerr << "missing required flags for apply\n";
    return 1;
  }

  optix::core::Artifact suggest_art;
  std::string err;
  if (!optix::core::read_artifact_json(suggest->second, &suggest_art, &err)) {
    std::cerr << err << "\n";
    return 2;
  }
  const auto* suggestions = std::get_if<optix::core::Suggestions>(&suggest_art.data);
  if (!suggestions) {
    std::cerr << "apply input artifact is not Suggestions\n";
    return 3;
  }

  std::filesystem::path source_root = src_root->second;
  std::filesystem::path out_dir = out->second;
  std::filesystem::create_directories(out_dir / "backups");

  std::map<std::filesystem::path, std::vector<optix::core::OptimizationSuggestion>> grouped;
  for (const auto& s : *suggestions) {
    auto target = resolve_target_file(source_root, s.file);
    grouped[target].push_back(s);
  }

  int touched_files = 0;
  int inserted_hints = 0;
  std::ofstream summary(out_dir / "apply_summary.txt", std::ios::trunc);
  summary << "apply summary\n";

  for (auto& [file, rows] : grouped) {
    if (!std::filesystem::exists(file)) {
      summary << "skip missing file: " << file << "\n";
      continue;
    }
    std::ifstream ifs(file);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(ifs, line)) {
      lines.push_back(line);
    }

    std::sort(rows.begin(), rows.end(), [](const auto& a, const auto& b) {
      return a.line > b.line;
    });

    for (const auto& s : rows) {
      int idx = s.line <= 0 ? 0 : s.line - 1;
      if (idx > static_cast<int>(lines.size())) idx = static_cast<int>(lines.size());
      const std::string hint = "// OPTIX_APPLIED: " + s.title + " | " + s.explanation;
      lines.insert(lines.begin() + idx, hint);
      ++inserted_hints;
    }

    std::error_code ec;
    auto rel = std::filesystem::relative(file, source_root, ec);
    if (ec) rel.clear();
    const auto backup = out_dir / "backups" / (rel.empty() ? file.filename() : rel);
    std::filesystem::create_directories(backup.parent_path());
    std::filesystem::copy_file(file, backup, std::filesystem::copy_options::overwrite_existing);

    std::ofstream ofs(file, std::ios::trunc);
    for (size_t i = 0; i < lines.size(); ++i) {
      ofs << lines[i];
      if (i + 1 < lines.size()) ofs << "\n";
    }
    ++touched_files;
    summary << "patched: " << file << " suggestions=" << rows.size() << "\n";
  }

  summary << "touched_files=" << touched_files << "\n";
  summary << "inserted_hints=" << inserted_hints << "\n";

  std::cout << "apply done: touched_files=" << touched_files
            << " inserted_hints=" << inserted_hints << "\n";
  return 0;
}

// run_plugin_command 执行插件管理子命令。
int run_plugin_command(int argc, char** argv,
                       const std::unordered_map<std::string, std::string>& flags) {
  if (argc < 3) {
    std::cerr << "plugin subcommand required: list|validate|enable|disable\n";
    return 1;
  }

  const std::string sub = argv[2];
  optix::plugins::PluginManager pm;

  if (sub == "list") {
    auto it = flags.find("plugin");
    if (it != flags.end()) {
      std::string err;
      if (!pm.load(it->second, &err)) {
        std::cerr << "load failed: " << err << "\n";
        return 2;
      }
    }
    for (const auto& m : pm.list_manifests()) {
      std::cout << m.id << " version=" << m.version
                << " type=" << optix::plugins::plugin_type_to_string(m.type) << "\n";
    }
    return 0;
  }

  if (sub == "validate") {
    auto it = flags.find("path");
    if (it == flags.end()) {
      std::cerr << "--path is required\n";
      return 3;
    }
    std::string err;
    if (!pm.validate(it->second, &err)) {
      std::cerr << "invalid plugin: " << err << "\n";
      return 4;
    }
    std::cout << "plugin valid\n";
    return 0;
  }

  if (sub == "enable" || sub == "disable") {
    const auto it = flags.find("id");
    if (it == flags.end()) {
      std::cerr << "--id is required\n";
      return 6;
    }
    const auto cfg_it = flags.find("config");
    const std::filesystem::path cfg =
        cfg_it == flags.end() ? std::filesystem::path(".optix_plugins_enabled")
                              : std::filesystem::path(cfg_it->second);
    std::set<std::string> ids;
    {
      std::ifstream ifs(cfg);
      std::string line;
      while (std::getline(ifs, line)) {
        if (!line.empty()) ids.insert(line);
      }
    }
    if (sub == "enable") {
      ids.insert(it->second);
    } else {
      ids.erase(it->second);
    }
    std::ofstream ofs(cfg, std::ios::trunc);
    for (const auto& id : ids) {
      ofs << id << "\n";
    }
    std::cout << sub << " ok: " << it->second << " in " << cfg << "\n";
    return 0;
  }

  std::cerr << "unknown plugin subcommand\n";
  return 5;
}

// run_replay 展示指定 run_id 的阶段产物。
int run_replay(const std::unordered_map<std::string, std::string>& flags) {
  const auto run_id = flags.find("run-id");
  const auto out = flags.find("out");
  if (run_id == flags.end() || out == flags.end()) {
    std::cerr << "missing required flags for replay\n";
    return 1;
  }

  const auto dir = std::filesystem::path(out->second) / "artifacts" / run_id->second;
  if (!std::filesystem::exists(dir)) {
    std::cerr << "run artifacts not found: " << dir << "\n";
    return 2;
  }

  for (auto it = std::filesystem::directory_iterator(dir);
       it != std::filesystem::directory_iterator(); ++it) {
    if (!it->is_regular_file()) continue;
    std::cout << it->path().filename().string() << "\n";
  }
  return 0;
}

} // namespace

// main 是程序主入口，根据子命令进行分发执行。
int main(int argc, char** argv) {
  if (argc < 2) {
    print_help();
    return 0;
  }

  const std::string cmd = argv[1];
  const auto flags = parse_flags(argc, argv, 2);

  if (cmd == "run") return run_pipeline(flags);
  if (cmd == "ingest") return run_ingest(flags);
  if (cmd == "normalize") return run_normalize(flags);
  if (cmd == "index") return run_index(flags);
  if (cmd == "diagnose") return run_diagnose(flags);
  if (cmd == "suggest") return run_suggest(flags);
  if (cmd == "apply") return run_apply(flags);
  if (cmd == "plugin") return run_plugin_command(argc, argv, flags);
  if (cmd == "replay") return run_replay(flags);

  print_help();
  return 0;
}
