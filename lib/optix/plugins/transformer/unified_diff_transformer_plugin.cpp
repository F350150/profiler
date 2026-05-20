// unified_diff_transformer_plugin.cpp 实现统一 diff 补丁构建插件。
#include <sstream>

#include "optix/plugins/api.hpp"

namespace {

// UnifiedDiffTransformerPlugin 将机会点渲染为 diff 文本。
class UnifiedDiffTransformerPlugin final : public optix::plugins::ITransformerPlugin {
 public:
  // 返回插件清单信息。
  const optix::plugins::PluginManifest& manifest() const override { return manifest_; }

  // 生成统一 diff 格式补丁内容。
  std::string build_patch(const optix::core::Opportunities& opportunities) override {
    std::ostringstream oss;
    for (const auto& o : opportunities) {
      oss << "--- " << o.file << "\n";
      oss << "+++ " << o.file << "\n";
      oss << "@@ -" << o.line << ",1 +" << o.line << ",2 @@\n";
      oss << "+// OPTIX: " << o.recommendation << "\n";
      oss << "+// Pattern: " << o.pattern << "\n";
    }
    return oss.str();
  }

 private:
  optix::plugins::PluginManifest manifest_{
      "unified_diff_transformer", "0.1.0", "v1",
      optix::plugins::PluginType::transformer, "all", "c,cpp", "0.1.0"};
};

} // namespace

// 导出插件清单符号。
extern "C" const optix::plugins::PluginManifest* optix_plugin_manifest() {
  static UnifiedDiffTransformerPlugin plugin;
  return &plugin.manifest();
}

// 导出插件创建符号。
extern "C" void* optix_plugin_create() {
  return new UnifiedDiffTransformerPlugin();
}

// 导出插件销毁符号。
extern "C" void optix_plugin_destroy(void* p) {
  delete reinterpret_cast<UnifiedDiffTransformerPlugin*>(p);
}
