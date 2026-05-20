// plugin_manager.hpp 定义插件加载与查询管理器。
#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "optix/plugins/api.hpp"

namespace optix::plugins {

// LoadedPlugin 保存动态插件的句柄、清单与实例。
struct LoadedPlugin {
  std::filesystem::path path;
  PluginManifest manifest;
  void* handle{nullptr};
  void* instance{nullptr};
  DestroyFn destroy{nullptr};
};

// PluginManager 负责插件校验、加载与类型化获取。
class PluginManager {
 public:
  // 析构时会释放已加载插件资源。
  ~PluginManager();

  // 加载指定路径插件。
  bool load(const std::filesystem::path& path, std::string* error = nullptr);
  // 返回当前已加载插件清单。
  std::vector<PluginManifest> list_manifests() const;
  // 校验插件符号与清单合法性。
  bool validate(const std::filesystem::path& path, std::string* error = nullptr) const;

  // 获取 parser 类型插件实例（不存在则返回 nullptr）。
  IParserPlugin* parser() const;
  // 获取 rulepack 类型插件实例（不存在则返回 nullptr）。
  IRulePackPlugin* rulepack() const;
  // 获取 reporter 类型插件实例（不存在则返回 nullptr）。
  IReporterPlugin* reporter() const;
  // 获取 transformer 类型插件实例（不存在则返回 nullptr）。
  ITransformerPlugin* transformer() const;
  // 获取 advisor 类型插件实例（不存在则返回 nullptr）。
  IAdvisorPlugin* advisor() const;

 private:
  std::vector<LoadedPlugin> plugins_;
};

// 将插件类型转为可读字符串。
std::string plugin_type_to_string(PluginType type);

} // namespace optix::plugins
