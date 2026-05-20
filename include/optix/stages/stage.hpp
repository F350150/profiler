// stage.hpp 定义流水线阶段抽象与阶段描述结构。
#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "optix/core/artifact.hpp"
#include "optix/core/context.hpp"

namespace optix::stages {

// FailurePolicy 指定阶段失败后的处理策略。
enum class FailurePolicy {
  hard_fail,
  soft_fail,
  skip
};

// Stage 是所有流水线阶段实现的统一接口。
class Stage {
 public:
  virtual ~Stage() = default;
  // 返回阶段名称。
  virtual std::string name() const = 0;
  // 返回输入 schema 标识。
  virtual std::string input_schema() const = 0;
  // 返回输出 schema 标识。
  virtual std::string output_schema() const = 0;
  // 执行阶段逻辑并返回产物。
  virtual optix::core::Artifact run(const optix::core::ArtifactMap& deps,
                                    optix::core::Context& ctx) = 0;
};

// StageSpec 描述编排器注册阶段所需信息。
struct StageSpec {
  std::string id;
  std::vector<std::string> deps;
  FailurePolicy failure_policy{FailurePolicy::hard_fail};
  Stage* stage{nullptr};
};

} // namespace optix::stages
