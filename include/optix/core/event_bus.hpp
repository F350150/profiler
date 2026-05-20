// event_bus.hpp 定义事件总线，用于记录流水线运行事件。
#pragma once

#include <filesystem>
#include <mutex>
#include <string>

namespace optix::core {

// EventType 描述流水线中的关键事件类型。
enum class EventType {
  stage_start,
  stage_end,
  rule_hit,
  rule_skip,
  conflict_resolved,
  error
};

// EventBus 将结构化事件追加写入 JSONL 文件。
class EventBus {
 public:
  // 使用目标日志文件初始化事件总线。
  explicit EventBus(std::filesystem::path file);

  // 发布一条事件记录。
  void publish(EventType type, const std::string& stage,
               const std::string& message);

 private:
  std::filesystem::path file_;
  std::mutex mu_;
};

// 将事件类型转成可读字符串。
std::string to_string(EventType type);

} // namespace optix::core
