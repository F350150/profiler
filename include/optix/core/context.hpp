// context.hpp 定义流水线执行上下文与输入参数。
#pragma once

#include <filesystem>
#include <memory>
#include <string>

#include "optix/core/event_bus.hpp"

namespace optix::plugins {
class PluginManager;
class IParserPlugin;
class IRulePackPlugin;
class IReporterPlugin;
class ITransformerPlugin;
class IAdvisorPlugin;
} // namespace optix::plugins

namespace optix::core {

// PipelineInputs 汇总一次分析流程的输入路径参数。
struct PipelineInputs {
  std::filesystem::path ksys_path;
  std::filesystem::path source_root;
  std::filesystem::path compile_commands;
  std::filesystem::path rules_dir;
  std::filesystem::path output_dir;
};

// Context 在各 Stage 间共享运行时状态与插件句柄。
struct Context {
  std::string run_id;
  PipelineInputs inputs;
  EventBus* event_bus{nullptr};
  optix::plugins::PluginManager* plugin_manager{nullptr};
  optix::plugins::IParserPlugin* parser_plugin{nullptr};
  optix::plugins::IRulePackPlugin* rulepack_plugin{nullptr};
  optix::plugins::IReporterPlugin* reporter_plugin{nullptr};
  optix::plugins::ITransformerPlugin* transformer_plugin{nullptr};
  optix::plugins::IAdvisorPlugin* advisor_plugin{nullptr};
};

} // namespace optix::core
