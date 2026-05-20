// markdown_reporter_plugin.cpp 实现 Markdown/JSON 报告插件。
#include <sstream>

#include "optix/plugins/api.hpp"

namespace {

// MarkdownReporterPlugin 将建议渲染为文本和 JSON。
class MarkdownReporterPlugin final : public optix::plugins::IReporterPlugin {
 public:
  // 返回插件清单信息。
  const optix::plugins::PluginManifest& manifest() const override { return manifest_; }

  // 渲染 Markdown 报告。
  std::string render_markdown(const optix::core::Suggestions& suggestions) override {
    std::ostringstream oss;
    oss << "# optix report\n\n";
    for (const auto& s : suggestions) {
      oss << "## " << s.title << "\n";
      oss << "- location: " << s.file << ":" << s.line << "\n";
      oss << "- expected_gain: " << s.expected_gain << "\n";
      oss << "- risk: " << s.risk_level << "\n";
      oss << "- explanation: " << s.explanation << "\n\n";
    }
    return oss.str();
  }

  // 渲染 JSON 报告。
  std::string render_json(const optix::core::Suggestions& suggestions) override {
    std::ostringstream oss;
    oss << "{\n  \"suggestions\": [\n";
    for (size_t i = 0; i < suggestions.size(); ++i) {
      const auto& s = suggestions[i];
      oss << "    {\"id\":\"" << s.id << "\",\"file\":\"" << s.file
          << "\",\"line\":" << s.line << ",\"title\":\"" << s.title
          << "\",\"risk\":\"" << s.risk_level << "\",\"expected_gain\":\""
          << s.expected_gain << "\"}";
      if (i + 1 < suggestions.size()) oss << ',';
      oss << "\n";
    }
    oss << "  ]\n}\n";
    return oss.str();
  }

 private:
  optix::plugins::PluginManifest manifest_{
      "markdown_reporter", "0.1.0", "v1", optix::plugins::PluginType::reporter,
      "all", "all", "0.1.0"};
};

} // namespace

// 导出插件清单符号。
extern "C" const optix::plugins::PluginManifest* optix_plugin_manifest() {
  static MarkdownReporterPlugin plugin;
  return &plugin.manifest();
}

// 导出插件创建符号。
extern "C" void* optix_plugin_create() {
  return new MarkdownReporterPlugin();
}

// 导出插件销毁符号。
extern "C" void optix_plugin_destroy(void* p) {
  delete reinterpret_cast<MarkdownReporterPlugin*>(p);
}
