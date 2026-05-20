// basic_advisor_plugin.cpp 提供基础建议增强插件实现。
#include "optix/plugins/api.hpp"

namespace {

// BasicAdvisorPlugin 在建议输出前补充验证与风险提示。
class BasicAdvisorPlugin final : public optix::plugins::IAdvisorPlugin {
 public:
  // 返回插件清单信息。
  const optix::plugins::PluginManifest& manifest() const override { return manifest_; }

  // 对建议列表做原地增强。
  void enrich(optix::core::Suggestions* suggestions) override {
    if (!suggestions) return;
    for (auto& s : *suggestions) {
      s.explanation += " [advisor: validate with perf benchmark and regression tests]";
      if (s.risk_level == "medium") {
        s.expected_gain += "; verify behavior on boundary inputs";
      }
    }
  }

 private:
  optix::plugins::PluginManifest manifest_{
      "basic_advisor", "0.1.0", "v1", optix::plugins::PluginType::advisor,
      "all", "all", "0.1.0"};
};

} // namespace

// 导出插件清单符号。
extern "C" const optix::plugins::PluginManifest* optix_plugin_manifest() {
  static BasicAdvisorPlugin plugin;
  return &plugin.manifest();
}

// 导出插件创建符号。
extern "C" void* optix_plugin_create() {
  return new BasicAdvisorPlugin();
}

// 导出插件销毁符号。
extern "C" void optix_plugin_destroy(void* p) {
  delete reinterpret_cast<BasicAdvisorPlugin*>(p);
}
