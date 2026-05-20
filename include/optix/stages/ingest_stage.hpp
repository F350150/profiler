// ingest_stage.hpp 定义采集结果接入阶段。
#pragma once

#include "optix/stages/stage.hpp"

namespace optix::stages {

// IngestStage 负责将 ksys 等输入解析为原始指标记录。
class IngestStage : public Stage {
 public:
  std::string name() const override { return "ingest"; }
  std::string input_schema() const override { return "none"; }
  std::string output_schema() const override { return "RawMetricRecords@v1.0"; }
  optix::core::Artifact run(const optix::core::ArtifactMap& deps,
                            optix::core::Context& ctx) override;
};

} // namespace optix::stages
