// artifact.hpp 定义流水线阶段之间传递的统一产物结构。
#pragma once

#include <string>
#include <unordered_map>
#include <variant>

#include "optix/core/types.hpp"

namespace optix::core {

// ArtifactData 表示某个阶段可输出的数据联合类型。
using ArtifactData = std::variant<std::monostate, RawMetricRecords, Signals, Semantics,
                                  CodeFactGraph, Opportunities, Suggestions>;

// Artifact 封装单个阶段输出，包含阶段标识、元数据与业务数据。
struct Artifact {
  std::string stage_id;
  Metadata meta;
  ArtifactData data;
};

// ArtifactMap 保存各阶段名到产物的映射。
using ArtifactMap = std::unordered_map<std::string, Artifact>;

} // namespace optix::core
