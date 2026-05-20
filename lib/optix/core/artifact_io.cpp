// artifact_io.cpp 实现 Artifact 的轻量序列化与反序列化。
#include "optix/core/artifact_io.hpp"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace optix::core {
namespace {

// escape_json 对字符串做最小 JSON 转义。
std::string escape_json(const std::string& in) {
  std::string out;
  out.reserve(in.size() + 8);
  for (char c : in) {
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default: out += c; break;
    }
  }
  return out;
}

// unescape_json 还原转义后的 JSON 字符串。
std::string unescape_json(std::string in) {
  std::string out;
  out.reserve(in.size());
  for (size_t i = 0; i < in.size(); ++i) {
    if (in[i] == '\\' && i + 1 < in.size()) {
      const char n = in[++i];
      switch (n) {
        case 'n': out += '\n'; break;
        case 'r': out += '\r'; break;
        case 't': out += '\t'; break;
        case '\\': out += '\\'; break;
        case '"': out += '"'; break;
        default: out += n; break;
      }
    } else {
      out += in[i];
    }
  }
  return out;
}

// split 按分隔符拆分字符串。
std::vector<std::string> split(const std::string& s, char delim) {
  std::vector<std::string> out;
  std::string cur;
  for (char c : s) {
    if (c == delim) {
      out.push_back(cur);
      cur.clear();
    } else {
      cur.push_back(c);
    }
  }
  out.push_back(cur);
  return out;
}

// join 按分隔符拼接字符串列表。
std::string join(const std::vector<std::string>& parts, char delim) {
  std::ostringstream oss;
  for (size_t i = 0; i < parts.size(); ++i) {
    if (i > 0) {
      oss << delim;
    }
    oss << parts[i];
  }
  return oss.str();
}

std::string extract_string_field(const std::string& src, const std::string& key) {
  const std::string pattern = "\"" + key + "\":\"";
  const size_t start = src.find(pattern);
  if (start == std::string::npos) {
    return "";
  }
  size_t pos = start + pattern.size();
  std::string out;
  while (pos < src.size()) {
    if (src[pos] == '"' && src[pos - 1] != '\\') {
      break;
    }
    out.push_back(src[pos++]);
  }
  return unescape_json(out);
}

std::string extract_data_type(const ArtifactData& data) {
  if (std::holds_alternative<RawMetricRecords>(data)) return "RawMetricRecords";
  if (std::holds_alternative<Signals>(data)) return "Signals";
  if (std::holds_alternative<Semantics>(data)) return "Semantics";
  if (std::holds_alternative<CodeFactGraph>(data)) return "CodeFactGraph";
  if (std::holds_alternative<Opportunities>(data)) return "Opportunities";
  if (std::holds_alternative<Suggestions>(data)) return "Suggestions";
  return "None";
}

void write_record_lines(const ArtifactData& data, std::ostream& os) {
  auto write_lines = [&](const std::vector<std::string>& lines) {
    for (const auto& line : lines) {
      os << "    \"" << escape_json(line) << "\",\n";
    }
  };

  if (const auto* recs = std::get_if<RawMetricRecords>(&data)) {
    std::vector<std::string> lines;
    for (const auto& r : *recs) {
      lines.emplace_back(join({r.domain, r.metric, std::to_string(r.value), r.unit, r.scope, r.window}, '|'));
    }
    write_lines(lines);
    return;
  }
  if (const auto* sigs = std::get_if<Signals>(&data)) {
    std::vector<std::string> lines;
    for (const auto& s : *sigs) {
      lines.emplace_back(join({s.id, s.domain, s.metric, s.scope, s.window, std::to_string(s.value),
                               std::to_string(s.baseline), s.severity, std::to_string(s.confidence)}, '|'));
    }
    write_lines(lines);
    return;
  }
  if (const auto* sems = std::get_if<Semantics>(&data)) {
    std::vector<std::string> lines;
    for (const auto& s : *sems) {
      lines.emplace_back(join({s.id, s.domain, s.semantic, s.severity, std::to_string(s.confidence),
                               join(s.evidence_metrics, ';'), s.reason}, '|'));
    }
    write_lines(lines);
    return;
  }
  if (const auto* graph = std::get_if<CodeFactGraph>(&data)) {
    std::vector<std::string> lines;
    for (const auto& f : graph->functions) {
      lines.emplace_back(join({"FUNC", f.file, f.function, std::to_string(f.line)}, '|'));
    }
    for (const auto& l : graph->loops) {
      lines.emplace_back(join({"LOOP", l.file, l.function, std::to_string(l.line), l.has_contiguous_access ? "1" : "0"}, '|'));
    }
    for (const auto& m : graph->memory_accesses) {
      lines.emplace_back(join({"MEM", m.file, m.function, std::to_string(m.line), m.pattern}, '|'));
    }
    for (const auto& i : graph->io_calls) {
      lines.emplace_back(join({"IO", i.file, i.function, std::to_string(i.line), i.call_name}, '|'));
    }
    for (const auto& n : graph->net_calls) {
      lines.emplace_back(join({"NET", n.file, n.function, std::to_string(n.line), n.call_name}, '|'));
    }
    for (const auto& l : graph->locks) {
      lines.emplace_back(join({"LOCK", l.file, l.function, std::to_string(l.line), l.primitive}, '|'));
    }
    write_lines(lines);
    return;
  }
  if (const auto* ops = std::get_if<Opportunities>(&data)) {
    std::vector<std::string> lines;
    for (const auto& o : *ops) {
      lines.emplace_back(join({o.id, o.semantic_id, o.file, o.function, std::to_string(o.line), o.pattern,
                               o.title, o.recommendation, std::to_string(o.benefit_score),
                               std::to_string(o.risk_score), std::to_string(o.confidence_score),
                               o.exclusive_group, std::to_string(o.priority)}, '|'));
    }
    write_lines(lines);
    return;
  }
  if (const auto* ss = std::get_if<Suggestions>(&data)) {
    std::vector<std::string> lines;
    for (const auto& s : *ss) {
      lines.emplace_back(join({s.id, s.file, std::to_string(s.line), s.title, s.explanation,
                               s.expected_gain, s.risk_level, s.patch}, '|'));
    }
    write_lines(lines);
    return;
  }
}

std::vector<std::string> extract_records(const std::string& src) {
  std::vector<std::string> lines;
  const auto arr_pos = src.find("\"records\":[");
  if (arr_pos == std::string::npos) {
    return lines;
  }
  size_t pos = src.find('[', arr_pos);
  if (pos == std::string::npos) return lines;
  ++pos;

  while (pos < src.size()) {
    while (pos < src.size() &&
           (src[pos] == ' ' || src[pos] == '\n' || src[pos] == '\r' ||
            src[pos] == '\t' || src[pos] == ',')) {
      ++pos;
    }
    if (pos >= src.size() || src[pos] == ']') {
      break;
    }
    if (src[pos] != '"') {
      ++pos;
      continue;
    }
    ++pos;
    std::string s;
    while (pos < src.size()) {
      if (src[pos] == '"' && src[pos - 1] != '\\') {
        break;
      }
      s.push_back(src[pos++]);
    }
    lines.push_back(unescape_json(s));
    if (pos < src.size()) ++pos;
  }
  return lines;
}

} // namespace

// write_artifact_json 将 Artifact 写入 JSON 文件。
bool write_artifact_json(const Artifact& artifact, const std::filesystem::path& path,
                         std::string* error) {
  std::ofstream ofs(path);
  if (!ofs) {
    if (error) *error = "failed to open file for write: " + path.string();
    return false;
  }
  ofs << "{\n";
  ofs << "  \"stage_id\":\"" << escape_json(artifact.stage_id) << "\",\n";
  ofs << "  \"schema_version\":\"" << escape_json(artifact.meta.schema_version) << "\",\n";
  ofs << "  \"producer\":\"" << escape_json(artifact.meta.producer) << "\",\n";
  ofs << "  \"run_id\":\"" << escape_json(artifact.meta.run_id) << "\",\n";
  ofs << "  \"timestamp\":\"" << escape_json(artifact.meta.timestamp) << "\",\n";
  ofs << "  \"data_type\":\"" << extract_data_type(artifact.data) << "\",\n";
  ofs << "  \"records\":[\n";
  write_record_lines(artifact.data, ofs);
  ofs << "  ]\n";
  ofs << "}\n";
  return true;
}

// read_artifact_json 从 JSON 文件读取 Artifact。
bool read_artifact_json(const std::filesystem::path& path, Artifact* artifact,
                        std::string* error) {
  std::ifstream ifs(path);
  if (!ifs) {
    if (error) *error = "failed to open file for read: " + path.string();
    return false;
  }
  std::stringstream buffer;
  buffer << ifs.rdbuf();
  const std::string src = buffer.str();

  Artifact out;
  out.stage_id = extract_string_field(src, "stage_id");
  out.meta.schema_version = extract_string_field(src, "schema_version");
  out.meta.producer = extract_string_field(src, "producer");
  out.meta.run_id = extract_string_field(src, "run_id");
  out.meta.timestamp = extract_string_field(src, "timestamp");

  const std::string dtype = extract_string_field(src, "data_type");
  const auto recs = extract_records(src);

  if (dtype == "RawMetricRecords") {
    RawMetricRecords rows;
    for (const auto& line : recs) {
      auto p = split(line, '|');
      if (p.size() < 6) continue;
      RawMetricRecord r;
      r.meta = out.meta;
      r.domain = p[0];
      r.metric = p[1];
      r.value = std::stod(p[2]);
      r.unit = p[3];
      r.scope = p[4];
      r.window = p[5];
      rows.push_back(std::move(r));
    }
    out.data = std::move(rows);
  } else if (dtype == "Signals") {
    Signals rows;
    for (const auto& line : recs) {
      auto p = split(line, '|');
      if (p.size() < 9) continue;
      Signal s;
      s.meta = out.meta;
      s.id = p[0]; s.domain = p[1]; s.metric = p[2]; s.scope = p[3]; s.window = p[4];
      s.value = std::stod(p[5]); s.baseline = std::stod(p[6]); s.severity = p[7];
      s.confidence = std::stod(p[8]);
      rows.push_back(std::move(s));
    }
    out.data = std::move(rows);
  } else if (dtype == "Semantics") {
    Semantics rows;
    for (const auto& line : recs) {
      auto p = split(line, '|');
      if (p.size() < 7) continue;
      BottleneckSemantic s;
      s.meta = out.meta;
      s.id = p[0]; s.domain = p[1]; s.semantic = p[2]; s.severity = p[3];
      s.confidence = std::stod(p[4]); s.evidence_metrics = split(p[5], ';'); s.reason = p[6];
      rows.push_back(std::move(s));
    }
    out.data = std::move(rows);
  } else if (dtype == "CodeFactGraph") {
    CodeFactGraph g;
    g.meta = out.meta;
    for (const auto& line : recs) {
      auto p = split(line, '|');
      if (p.size() < 4) continue;
      if (p[0] == "FUNC" && p.size() >= 4) {
        g.functions.push_back({p[1], p[2], std::stoi(p[3])});
      } else if (p[0] == "LOOP" && p.size() >= 5) {
        g.loops.push_back({p[1], p[2], std::stoi(p[3]), p[4] == "1"});
      } else if (p[0] == "MEM" && p.size() >= 5) {
        g.memory_accesses.push_back({p[1], p[2], std::stoi(p[3]), p[4]});
      } else if (p[0] == "IO" && p.size() >= 5) {
        g.io_calls.push_back({p[1], p[2], std::stoi(p[3]), p[4]});
      } else if (p[0] == "NET" && p.size() >= 5) {
        g.net_calls.push_back({p[1], p[2], std::stoi(p[3]), p[4]});
      } else if (p[0] == "LOCK" && p.size() >= 5) {
        g.locks.push_back({p[1], p[2], std::stoi(p[3]), p[4]});
      }
    }
    out.data = std::move(g);
  } else if (dtype == "Opportunities") {
    Opportunities rows;
    for (const auto& line : recs) {
      auto p = split(line, '|');
      if (p.size() < 13) continue;
      CodeOpportunity o;
      o.meta = out.meta;
      o.id = p[0]; o.semantic_id = p[1]; o.file = p[2]; o.function = p[3]; o.line = std::stoi(p[4]);
      o.pattern = p[5]; o.title = p[6]; o.recommendation = p[7]; o.benefit_score = std::stod(p[8]);
      o.risk_score = std::stod(p[9]); o.confidence_score = std::stod(p[10]); o.exclusive_group = p[11];
      o.priority = std::stoi(p[12]);
      rows.push_back(std::move(o));
    }
    out.data = std::move(rows);
  } else if (dtype == "Suggestions") {
    Suggestions rows;
    for (const auto& line : recs) {
      auto p = split(line, '|');
      if (p.size() < 8) continue;
      OptimizationSuggestion s;
      s.meta = out.meta;
      s.id = p[0]; s.file = p[1]; s.line = std::stoi(p[2]); s.title = p[3]; s.explanation = p[4];
      s.expected_gain = p[5]; s.risk_level = p[6]; s.patch = p[7];
      rows.push_back(std::move(s));
    }
    out.data = std::move(rows);
  } else {
    out.data = std::monostate{};
  }

  *artifact = std::move(out);
  return true;
}

} // namespace optix::core
