// plugin_manager.cpp 实现插件动态加载、校验与实例查询。
#include "optix/plugins/plugin_manager.hpp"

#include <algorithm>

#if defined(OPTIX_PLATFORM_POSIX)
#include <dlfcn.h>
#endif

namespace optix::plugins {

// plugin_type_to_string 将插件类型枚举转换为可读字符串。
std::string plugin_type_to_string(PluginType type) {
  switch (type) {
    case PluginType::parser: return "parser";
    case PluginType::rulepack: return "rulepack";
    case PluginType::reporter: return "reporter";
    case PluginType::transformer: return "transformer";
    case PluginType::advisor: return "advisor";
  }
  return "unknown";
}

// 析构时释放所有已加载插件实例与动态库句柄。
PluginManager::~PluginManager() {
#if defined(OPTIX_PLATFORM_POSIX)
  for (auto& p : plugins_) {
    if (p.destroy && p.instance) {
      p.destroy(p.instance);
    }
    if (p.handle) {
      dlclose(p.handle);
    }
  }
#endif
}

// validate 仅做插件 ABI 与清单合法性检查。
bool PluginManager::validate(const std::filesystem::path& path,
                             std::string* error) const {
#if !defined(OPTIX_PLATFORM_POSIX)
  if (error) *error = "dynamic plugin validation currently supported on POSIX only";
  return false;
#else
  void* handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
  if (!handle) {
    if (error) *error = dlerror();
    return false;
  }
  auto manifest_fn = reinterpret_cast<ManifestFn>(dlsym(handle, "optix_plugin_manifest"));
  if (!manifest_fn) {
    if (error) *error = "missing symbol: optix_plugin_manifest";
    dlclose(handle);
    return false;
  }
  const PluginManifest* manifest = manifest_fn();
  if (!manifest || !manifest->id || !manifest->engine_api) {
    if (error) *error = "invalid manifest";
    dlclose(handle);
    return false;
  }
  dlclose(handle);
  return true;
#endif
}

// load 加载并实例化指定插件。
bool PluginManager::load(const std::filesystem::path& path, std::string* error) {
#if !defined(OPTIX_PLATFORM_POSIX)
  if (error) *error = "dynamic plugin loading currently supported on POSIX only";
  return false;
#else
  void* handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
  if (!handle) {
    if (error) *error = dlerror();
    return false;
  }

  auto manifest_fn = reinterpret_cast<ManifestFn>(dlsym(handle, "optix_plugin_manifest"));
  auto create_fn = reinterpret_cast<CreateFn>(dlsym(handle, "optix_plugin_create"));
  auto destroy_fn = reinterpret_cast<DestroyFn>(dlsym(handle, "optix_plugin_destroy"));

  if (!manifest_fn || !create_fn || !destroy_fn) {
    if (error) *error = "plugin ABI symbols missing";
    dlclose(handle);
    return false;
  }

  const PluginManifest* m = manifest_fn();
  if (!m || !m->id || !m->engine_api) {
    if (error) *error = "invalid plugin manifest";
    dlclose(handle);
    return false;
  }

  LoadedPlugin p;
  p.path = path;
  p.manifest = *m;
  p.handle = handle;
  p.instance = create_fn();
  p.destroy = destroy_fn;
  plugins_.push_back(std::move(p));
  return true;
#endif
}

// list_manifests 返回当前已加载插件清单。
std::vector<PluginManifest> PluginManager::list_manifests() const {
  std::vector<PluginManifest> out;
  out.reserve(plugins_.size());
  for (const auto& p : plugins_) {
    out.push_back(p.manifest);
  }
  return out;
}

// parser 返回 parser 类型插件实例。
IParserPlugin* PluginManager::parser() const {
  for (const auto& p : plugins_) {
    if (p.manifest.type == PluginType::parser) {
      return reinterpret_cast<IParserPlugin*>(p.instance);
    }
  }
  return nullptr;
}

// rulepack 返回 rulepack 类型插件实例。
IRulePackPlugin* PluginManager::rulepack() const {
  for (const auto& p : plugins_) {
    if (p.manifest.type == PluginType::rulepack) {
      return reinterpret_cast<IRulePackPlugin*>(p.instance);
    }
  }
  return nullptr;
}

// reporter 返回 reporter 类型插件实例。
IReporterPlugin* PluginManager::reporter() const {
  for (const auto& p : plugins_) {
    if (p.manifest.type == PluginType::reporter) {
      return reinterpret_cast<IReporterPlugin*>(p.instance);
    }
  }
  return nullptr;
}

// transformer 返回 transformer 类型插件实例。
ITransformerPlugin* PluginManager::transformer() const {
  for (const auto& p : plugins_) {
    if (p.manifest.type == PluginType::transformer) {
      return reinterpret_cast<ITransformerPlugin*>(p.instance);
    }
  }
  return nullptr;
}

// advisor 返回 advisor 类型插件实例。
IAdvisorPlugin* PluginManager::advisor() const {
  for (const auto& p : plugins_) {
    if (p.manifest.type == PluginType::advisor) {
      return reinterpret_cast<IAdvisorPlugin*>(p.instance);
    }
  }
  return nullptr;
}

} // namespace optix::plugins
