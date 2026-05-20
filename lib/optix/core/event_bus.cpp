// event_bus.cpp 实现流水线事件日志输出。
#include "optix/core/event_bus.hpp"

#include <fstream>

#include "optix/core/metadata.hpp"

namespace optix::core {

// EventBus 构造函数，绑定事件输出文件路径。
EventBus::EventBus(std::filesystem::path file) : file_(std::move(file)) {}

// to_string 将事件枚举转换为字符串。
std::string to_string(EventType type) {
  switch (type) {
    case EventType::stage_start: return "stage_start";
    case EventType::stage_end: return "stage_end";
    case EventType::rule_hit: return "rule_hit";
    case EventType::rule_skip: return "rule_skip";
    case EventType::conflict_resolved: return "conflict_resolved";
    case EventType::error: return "error";
  }
  return "unknown";
}

// publish 将事件追加写入 JSONL 文件。
void EventBus::publish(EventType type, const std::string& stage,
                       const std::string& message) {
  std::lock_guard<std::mutex> lock(mu_);
  std::ofstream ofs(file_, std::ios::app);
  ofs << "{"
      << "\"ts\":\"" << now_utc_rfc3339() << "\","
      << "\"event\":\"" << to_string(type) << "\","
      << "\"stage\":\"" << stage << "\","
      << "\"message\":\"" << message << "\""
      << "}\n";
}

} // namespace optix::core
