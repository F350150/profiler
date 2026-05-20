// api.hpp 定义插件 ABI、插件类型与插件接口。
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "optix/core/types.hpp"

namespace optix::plugins {

// PluginType 描述插件在流水线中的职责类型。
enum class PluginType : std::uint32_t {
  parser = 1,
  rulepack = 2,
  reporter = 3,
  transformer = 4,
  advisor = 5
};

// PluginManifest 是插件元数据声明结构。
struct PluginManifest {
  const char* id;
  const char* version;
  const char* engine_api;
  PluginType type;
  const char* domains;
  const char* language_targets;
  const char* min_core_version;
};

// IPlugin 是所有插件接口的公共基类。
class IPlugin {
 public:
  virtual ~IPlugin() = default;
  // 返回插件清单信息。
  virtual const PluginManifest& manifest() const = 0;
};

// IParserPlugin 负责将外部数据解析为 RawMetricRecords。
class IParserPlugin : public IPlugin {
 public:
  // 解析输入路径并输出原始指标记录。
  virtual optix::core::RawMetricRecords parse(const std::string& path) = 0;
};

// IRulePackPlugin 负责语义与代码事实匹配。
class IRulePackPlugin : public IPlugin {
 public:
  // 基于语义和代码事实生成优化机会点。
  virtual optix::core::Opportunities match(
      const optix::core::Semantics& semantics,
      const optix::core::CodeFactGraph& graph) = 0;
};

// IReporterPlugin 负责渲染建议输出。
class IReporterPlugin : public IPlugin {
 public:
  // 生成 Markdown 报告。
  virtual std::string render_markdown(
      const optix::core::Suggestions& suggestions) = 0;
  // 生成 JSON 报告。
  virtual std::string render_json(const optix::core::Suggestions& suggestions) = 0;
};

// ITransformerPlugin 负责构建补丁文本。
class ITransformerPlugin : public IPlugin {
 public:
  // 将机会点转换为补丁文本。
  virtual std::string build_patch(
      const optix::core::Opportunities& opportunities) = 0;
};

// IAdvisorPlugin 为 AI/专家增强建议预留扩展口。
class IAdvisorPlugin : public IPlugin {
 public:
  // 原地增强建议内容（解释、风险、验证提示等）。
  virtual void enrich(optix::core::Suggestions* suggestions) = 0;
};

// 动态加载时使用的 C ABI 函数签名。
using ManifestFn = const PluginManifest* (*)();
using CreateFn = void* (*)();
using DestroyFn = void (*)(void*);

} // namespace optix::plugins

extern "C" {
// 返回插件清单信息。
const optix::plugins::PluginManifest* optix_plugin_manifest();
// 创建插件实例。
void* optix_plugin_create();
// 销毁插件实例。
void optix_plugin_destroy(void*);
}
