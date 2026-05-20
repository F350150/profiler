// artifact_io.hpp 提供 Artifact 的读写序列化接口。
#pragma once

#include <filesystem>
#include <string>

#include "optix/core/artifact.hpp"

namespace optix::core {

// 将 Artifact 写入指定路径，返回是否成功。
bool write_artifact_json(const Artifact& artifact, const std::filesystem::path& path,
                         std::string* error = nullptr);
// 从指定路径读取 Artifact，返回是否成功。
bool read_artifact_json(const std::filesystem::path& path, Artifact* artifact,
                        std::string* error = nullptr);

} // namespace optix::core
