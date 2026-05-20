// metadata.hpp 提供元数据生成与时间工具函数。
#pragma once

#include <string>

#include "optix/core/types.hpp"

namespace optix::core {

// 返回当前 UTC 时间（RFC3339 格式）。
std::string now_utc_rfc3339();
// 生成一次运行的唯一 run_id。
std::string make_run_id();
// 组装统一元数据对象。
Metadata make_metadata(const std::string& producer, const std::string& run_id,
                       const std::string& schema_version = "v1.0");

} // namespace optix::core
