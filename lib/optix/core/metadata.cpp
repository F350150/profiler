// metadata.cpp 实现统一元数据与时间工具函数。
#include "optix/core/metadata.hpp"

#include <chrono>
#include <iomanip>
#include <random>
#include <sstream>

namespace optix::core {

// now_utc_rfc3339 返回当前 UTC 时间字符串。
std::string now_utc_rfc3339() {
  using clock = std::chrono::system_clock;
  const auto now = clock::now();
  const auto t = clock::to_time_t(now);
  std::tm tm{};
#if defined(_WIN32)
  gmtime_s(&tm, &t);
#else
  gmtime_r(&t, &tm);
#endif
  std::ostringstream oss;
  oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
  return oss.str();
}

// make_run_id 生成一次执行的随机 run_id。
std::string make_run_id() {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<int> dist(0, 15);
  std::ostringstream oss;
  oss << "run-";
  for (int i = 0; i < 12; ++i) {
    oss << std::hex << dist(gen);
  }
  return oss.str();
}

// make_metadata 构建统一的 Metadata 结构体。
Metadata make_metadata(const std::string& producer, const std::string& run_id,
                       const std::string& schema_version) {
  Metadata m;
  m.schema_version = schema_version;
  m.producer = producer;
  m.run_id = run_id;
  m.timestamp = now_utc_rfc3339();
  return m;
}

} // namespace optix::core
