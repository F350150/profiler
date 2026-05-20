// suggest_stage.cpp 实现建议、补丁和报告生成。
#include "optix/stages/suggest_stage.hpp"

#include <fstream>
#include <stdexcept>

#include "optix/core/metadata.hpp"
#include "optix/plugins/api.hpp"

namespace optix::stages {
namespace {

// default_patch 生成默认建议补丁片段。
std::string default_patch(const optix::core::CodeOpportunity& o) {
  std::string file = o.file;
  if (!file.empty() && file[0] == '/') {
    // keep absolute path as-is
  }
  std::string patch;
  patch += "*** candidate patch for: " + o.file + "\n";
  patch += "@@ line " + std::to_string(o.line) + " @@\n";
  patch += "+ // OPTIX suggestion: " + o.recommendation + "\n";
  return patch;
}

// default_markdown 渲染默认 Markdown 报告。
std::string default_markdown(const optix::core::Suggestions& suggestions) {
  std::string out = "# optix suggestions\n\n";
  for (const auto& s : suggestions) {
    out += "## " + s.title + "\n";
    out += "- file: " + s.file + ":" + std::to_string(s.line) + "\n";
    out += "- expected_gain: " + s.expected_gain + "\n";
    out += "- risk: " + s.risk_level + "\n";
    out += "- explanation: " + s.explanation + "\n\n";
  }
  return out;
}

// default_json 渲染默认 JSON 报告。
std::string default_json(const optix::core::Suggestions& suggestions) {
  std::string out = "{\n  \"suggestions\": [\n";
  for (size_t i = 0; i < suggestions.size(); ++i) {
    const auto& s = suggestions[i];
    out += "    {\"id\":\"" + s.id + "\",\"file\":\"" + s.file +
           "\",\"line\":" + std::to_string(s.line) +
           ",\"title\":\"" + s.title + "\",\"risk\":\"" + s.risk_level +
           "\",\"expected_gain\":\"" + s.expected_gain + "\"}";
    if (i + 1 < suggestions.size()) out += ",";
    out += "\n";
  }
  out += "  ]\n}\n";
  return out;
}

} // namespace

// run 执行建议阶段并输出建议产物与报告文件。
optix::core::Artifact SuggestStage::run(const optix::core::ArtifactMap& deps,
                                        optix::core::Context& ctx) {
  const auto it = deps.find("rule");
  if (it == deps.end()) {
    throw std::runtime_error("suggest requires rule artifact");
  }
  const auto* ops = std::get_if<optix::core::Opportunities>(&it->second.data);
  if (!ops) {
    throw std::runtime_error("suggest got invalid artifact type");
  }

  const std::string global_patch = ctx.transformer_plugin
                                       ? ctx.transformer_plugin->build_patch(*ops)
                                       : std::string();

  optix::core::Suggestions suggestions;
  int seq = 0;
  for (const auto& o : *ops) {
    optix::core::OptimizationSuggestion s;
    s.meta = optix::core::make_metadata("stage.suggest", ctx.run_id);
    s.id = "sug-" + std::to_string(seq++);
    s.file = o.file;
    s.line = o.line;
    s.title = o.title;
    s.explanation = o.recommendation;
    s.expected_gain = "benefit=" + std::to_string(o.benefit_score);
    s.risk_level = o.risk_score > 0.5 ? "high" : (o.risk_score > 0.3 ? "medium" : "low");
    s.patch = global_patch.empty() ? default_patch(o) : global_patch;
    suggestions.push_back(std::move(s));
  }

  if (ctx.advisor_plugin) {
    ctx.advisor_plugin->enrich(&suggestions);
  }

  std::filesystem::create_directories(ctx.inputs.output_dir);

  const auto patch_path = ctx.inputs.output_dir / "patch.diff";
  std::ofstream patch_ofs(patch_path);
  if (!global_patch.empty()) {
    patch_ofs << global_patch;
  } else {
    for (const auto& s : suggestions) {
      patch_ofs << s.patch << "\n";
    }
  }

  const std::string md = ctx.reporter_plugin
                             ? ctx.reporter_plugin->render_markdown(suggestions)
                             : default_markdown(suggestions);
  const std::string js = ctx.reporter_plugin
                             ? ctx.reporter_plugin->render_json(suggestions)
                             : default_json(suggestions);

  std::ofstream(ctx.inputs.output_dir / "report.md") << md;
  std::ofstream(ctx.inputs.output_dir / "report.json") << js;

  optix::core::Artifact out;
  out.stage_id = name();
  out.meta = optix::core::make_metadata("stage.suggest", ctx.run_id);
  out.data = std::move(suggestions);
  return out;
}

} // namespace optix::stages
