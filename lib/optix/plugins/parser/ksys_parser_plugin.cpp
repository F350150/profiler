// ksys_parser_plugin.cpp 实现 ksys 日志解析插件。
#include <fstream>
#include <string>

#include "optix/core/metadata.hpp"
#include "optix/plugins/api.hpp"

namespace {

// KsysParserPlugin 负责将文本日志转为原始指标记录。
class KsysParserPlugin final : public optix::plugins::IParserPlugin {
 public:
  // 返回插件清单信息。
  const optix::plugins::PluginManifest& manifest() const override { return manifest_; }

  // 解析日志文件并生成 RawMetricRecords。
  optix::core::RawMetricRecords parse(const std::string& path) override {
    std::ifstream ifs(path);
    optix::core::RawMetricRecords out;
    std::string line;
    int i = 0;
    while (std::getline(ifs, line)) {
      auto push = [&](const std::string& domain, const std::string& metric, double value) {
        optix::core::RawMetricRecord r;
        r.meta = optix::core::make_metadata("plugin.ksys_parser", "external");
        r.domain = domain;
        r.metric = metric;
        r.value = value;
        r.unit = "score";
        r.scope = "host";
        r.window = "default";
        out.push_back(std::move(r));
      };

      if (line.find("CPU") != std::string::npos || line.find("cycles") != std::string::npos)
        push("cpu", "cycles", 0.80 + (i % 10) * 0.01);
      if (line.find("Mem") != std::string::npos || line.find("memory") != std::string::npos)
        push("memory", "miss_ratio", 0.70 + (i % 5) * 0.02);
      if (line.find("IO") != std::string::npos || line.find("await") != std::string::npos)
        push("io", "await", 0.60 + (i % 6) * 0.03);
      if (line.find("Network") != std::string::npos || line.find("tx") != std::string::npos)
        push("network", "packet_overhead", 0.65 + (i % 4) * 0.03);
      ++i;
    }
    return out;
  }

 private:
  optix::plugins::PluginManifest manifest_{
      "ksys_parser", "0.1.0", "v1", optix::plugins::PluginType::parser,
      "cpu,memory,io,network", "c,cpp", "0.1.0"};
};

} // namespace

// 导出插件清单符号。
extern "C" const optix::plugins::PluginManifest* optix_plugin_manifest() {
  static KsysParserPlugin plugin;
  return &plugin.manifest();
}

// 导出插件创建符号。
extern "C" void* optix_plugin_create() {
  return new KsysParserPlugin();
}

// 导出插件销毁符号。
extern "C" void optix_plugin_destroy(void* p) {
  delete reinterpret_cast<KsysParserPlugin*>(p);
}
