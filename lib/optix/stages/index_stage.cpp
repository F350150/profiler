// index_stage.cpp 实现静态源码索引阶段（LibTooling 优先，文本扫描兜底）。
#include "optix/stages/index_stage.hpp"

#include <fstream>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>

#include "optix/core/metadata.hpp"

#if defined(OPTIX_HAS_LIBTOOLING)
#include <clang/AST/ASTContext.h>
#include <clang/AST/Decl.h>
#include <clang/AST/Expr.h>
#include <clang/AST/ParentMapContext.h>
#include <clang/AST/Stmt.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/JSONCompilationDatabase.h>
#include <clang/Tooling/Tooling.h>
#endif

namespace optix::stages {
namespace {

// is_cpp_file 判断是否为 C/C++ 源码文件。
bool is_cpp_file(const std::filesystem::path& path) {
  const auto ext = path.extension().string();
  return ext == ".c" || ext == ".cc" || ext == ".cpp" || ext == ".h" || ext == ".hpp";
}

// scan_source_file_text_fallback 使用文本匹配兜底提取代码事实。
void scan_source_file_text_fallback(const std::filesystem::path& file,
                                    optix::core::CodeFactGraph* graph) {
  std::ifstream ifs(file);
  if (!ifs) {
    return;
  }

  std::string line;
  int line_no = 0;
  std::string active_function = "<global>";
  while (std::getline(ifs, line)) {
    ++line_no;

    if ((line.find("int ") != std::string::npos || line.find("void ") != std::string::npos) &&
        line.find('(') != std::string::npos && line.find(')') != std::string::npos &&
        line.find('{') != std::string::npos) {
      auto name_start = line.find_last_of(" ", line.find('('));
      if (name_start != std::string::npos) {
        active_function = line.substr(name_start + 1, line.find('(') - name_start - 1);
      }
      graph->functions.push_back({file.string(), active_function, line_no});
    }

    if (line.find("for (") != std::string::npos || line.find("for(") != std::string::npos) {
      const bool contiguous = line.find("++") != std::string::npos ||
                              line.find("+= 1") != std::string::npos;
      graph->loops.push_back({file.string(), active_function, line_no, contiguous});
    }

    if (line.find("malloc(") != std::string::npos || line.find("new ") != std::string::npos ||
        line.find("memcpy(") != std::string::npos) {
      graph->memory_accesses.push_back({file.string(), active_function, line_no,
                                        "allocation_or_copy"});
    }

    if (line.find("read(") != std::string::npos || line.find("write(") != std::string::npos ||
        line.find("open(") != std::string::npos || line.find("fsync(") != std::string::npos) {
      graph->io_calls.push_back({file.string(), active_function, line_no, "sync_io"});
    }

    if (line.find("send(") != std::string::npos || line.find("recv(") != std::string::npos ||
        line.find("sendto(") != std::string::npos || line.find("recvfrom(") != std::string::npos) {
      graph->net_calls.push_back({file.string(), active_function, line_no, "socket_call"});
    }

    if (line.find("mutex") != std::string::npos || line.find("lock_guard") != std::string::npos) {
      graph->locks.push_back({file.string(), active_function, line_no, "mutex"});
    }
  }
}

#if defined(OPTIX_HAS_LIBTOOLING)

using namespace clang;
using namespace clang::ast_matchers;

// safe_line 获取节点在源码中的行号。
int safe_line(const SourceManager& sm, SourceLocation loc) {
  if (loc.isInvalid()) return 1;
  const auto presumed = sm.getPresumedLoc(loc);
  if (presumed.isInvalid()) return 1;
  return static_cast<int>(presumed.getLine());
}

// find_enclosing_function 尝试查找节点所在函数。
template <typename NodeT>
const FunctionDecl* find_enclosing_function(const NodeT* node, ASTContext* ctx) {
  if (!node || !ctx) return nullptr;

  DynTypedNode current = DynTypedNode::create(*node);
  for (int depth = 0; depth < 64; ++depth) {
    const auto parents = ctx->getParents(current);
    if (parents.empty()) break;

    const DynTypedNode& p = parents[0];
    if (const auto* fd = p.get<FunctionDecl>()) {
      return fd;
    }
    current = p;
  }
  return nullptr;
}

// IndexCollector 负责接收 AST matcher 回调并写入 CodeFactGraph。
class IndexCollector final : public MatchFinder::MatchCallback {
 public:
  explicit IndexCollector(optix::core::CodeFactGraph* graph) : graph_(graph) {}

  void run(const MatchFinder::MatchResult& result) override {
    if (!graph_ || !result.Context) return;
    const auto& sm = result.SourceManager ? *result.SourceManager : result.Context->getSourceManager();

    if (const auto* fd = result.Nodes.getNodeAs<FunctionDecl>("func")) {
      if (!fd->isThisDeclarationADefinition()) return;
      const std::string file = sm.getFilename(fd->getBeginLoc()).str();
      if (file.empty()) return;
      const int ln = safe_line(sm, fd->getBeginLoc());
      const std::string name = fd->getNameAsString();
      const std::string key = "F|" + file + "|" + name + "|" + std::to_string(ln);
      if (seen_.insert(key).second) {
        graph_->functions.push_back({file, name, ln});
      }
      return;
    }

    if (const auto* fs = result.Nodes.getNodeAs<ForStmt>("for_stmt")) {
      const std::string file = sm.getFilename(fs->getForLoc()).str();
      if (file.empty()) return;
      const int ln = safe_line(sm, fs->getForLoc());
      const auto* f = find_enclosing_function(fs, result.Context);
      const std::string fn = f ? f->getNameAsString() : "<global>";
      const std::string key = "L|" + file + "|" + std::to_string(ln);
      if (seen_.insert(key).second) {
        graph_->loops.push_back({file, fn, ln, true});
      }
      return;
    }

    if (const auto* ce = result.Nodes.getNodeAs<CallExpr>("mem_call")) {
      const std::string file = sm.getFilename(ce->getExprLoc()).str();
      if (file.empty()) return;
      const int ln = safe_line(sm, ce->getExprLoc());
      const auto* f = find_enclosing_function(ce, result.Context);
      const std::string fn = f ? f->getNameAsString() : "<global>";
      const std::string key = "M|" + file + "|" + std::to_string(ln);
      if (seen_.insert(key).second) {
        graph_->memory_accesses.push_back({file, fn, ln, "allocation_or_copy"});
      }
      return;
    }

    if (const auto* ne = result.Nodes.getNodeAs<CXXNewExpr>("new_expr")) {
      const std::string file = sm.getFilename(ne->getExprLoc()).str();
      if (file.empty()) return;
      const int ln = safe_line(sm, ne->getExprLoc());
      const auto* f = find_enclosing_function(ne, result.Context);
      const std::string fn = f ? f->getNameAsString() : "<global>";
      const std::string key = "M|" + file + "|" + std::to_string(ln);
      if (seen_.insert(key).second) {
        graph_->memory_accesses.push_back({file, fn, ln, "allocation_or_copy"});
      }
      return;
    }

    if (const auto* ce = result.Nodes.getNodeAs<CallExpr>("io_call")) {
      const std::string file = sm.getFilename(ce->getExprLoc()).str();
      if (file.empty()) return;
      const int ln = safe_line(sm, ce->getExprLoc());
      const auto* f = find_enclosing_function(ce, result.Context);
      const std::string fn = f ? f->getNameAsString() : "<global>";
      const std::string key = "I|" + file + "|" + std::to_string(ln);
      if (seen_.insert(key).second) {
        graph_->io_calls.push_back({file, fn, ln, "sync_io"});
      }
      return;
    }

    if (const auto* ce = result.Nodes.getNodeAs<CallExpr>("net_call")) {
      const std::string file = sm.getFilename(ce->getExprLoc()).str();
      if (file.empty()) return;
      const int ln = safe_line(sm, ce->getExprLoc());
      const auto* f = find_enclosing_function(ce, result.Context);
      const std::string fn = f ? f->getNameAsString() : "<global>";
      const std::string key = "N|" + file + "|" + std::to_string(ln);
      if (seen_.insert(key).second) {
        graph_->net_calls.push_back({file, fn, ln, "socket_call"});
      }
      return;
    }

    if (const auto* ce = result.Nodes.getNodeAs<CXXConstructExpr>("lock_ctor")) {
      const std::string file = sm.getFilename(ce->getExprLoc()).str();
      if (file.empty()) return;
      const int ln = safe_line(sm, ce->getExprLoc());
      const auto* f = find_enclosing_function(ce, result.Context);
      const std::string fn = f ? f->getNameAsString() : "<global>";
      const std::string key = "K|" + file + "|" + std::to_string(ln);
      if (seen_.insert(key).second) {
        graph_->locks.push_back({file, fn, ln, "mutex"});
      }
      return;
    }
  }

 private:
  optix::core::CodeFactGraph* graph_;
  std::set<std::string> seen_;
};

// read_file 读取文件全部文本。
std::string read_file(const std::filesystem::path& file) {
  std::ifstream ifs(file);
  if (!ifs) return "";
  std::stringstream buf;
  buf << ifs.rdbuf();
  return buf.str();
}

// make_compile_args 为单文件分析生成编译参数。
std::vector<std::string> make_compile_args(const std::filesystem::path& file,
                                           const std::filesystem::path& compile_commands_path) {
  std::vector<std::string> args = {"-std=c++20", "-x", "c++"};

  if (compile_commands_path.empty() || !std::filesystem::exists(compile_commands_path)) {
    return args;
  }

  std::string err;
  auto db = tooling::JSONCompilationDatabase::loadFromFile(
      compile_commands_path.string(), err,
      tooling::JSONCommandLineSyntax::AutoDetect);
  if (!db) {
    return args;
  }

  const auto cmds = db->getCompileCommands(file.string());
  if (cmds.empty()) {
    return args;
  }

  std::vector<std::string> from_db;
  for (size_t i = 0; i < cmds[0].CommandLine.size(); ++i) {
    // 跳过编译器二进制本身。
    if (i == 0) continue;
    from_db.push_back(cmds[0].CommandLine[i]);
  }
  if (!from_db.empty()) {
    return from_db;
  }
  return args;
}

// scan_source_file_libtooling 使用 LibTooling AST Matcher 提取代码事实。
bool scan_source_file_libtooling(const std::filesystem::path& file,
                                 const std::filesystem::path& compile_commands_path,
                                 optix::core::CodeFactGraph* graph) {
  const std::string code = read_file(file);
  if (code.empty()) {
    return false;
  }

  IndexCollector collector(graph);
  MatchFinder finder;

  finder.addMatcher(functionDecl(isDefinition()).bind("func"), &collector);
  finder.addMatcher(forStmt().bind("for_stmt"), &collector);

  finder.addMatcher(callExpr(callee(functionDecl(hasAnyName("malloc", "memcpy")))).bind("mem_call"),
                    &collector);
  finder.addMatcher(cxxNewExpr().bind("new_expr"), &collector);

  finder.addMatcher(callExpr(callee(functionDecl(hasAnyName("read", "write", "open", "fsync")))).bind("io_call"),
                    &collector);
  finder.addMatcher(callExpr(callee(functionDecl(hasAnyName("send", "recv", "sendto", "recvfrom")))).bind("net_call"),
                    &collector);

  finder.addMatcher(cxxConstructExpr(hasType(cxxRecordDecl(matchesName("(mutex|lock_guard)"))))
                        .bind("lock_ctor"),
                    &collector);

  auto action_factory = tooling::newFrontendActionFactory(&finder);
  const auto args = make_compile_args(file, compile_commands_path);
  return tooling::runToolOnCodeWithArgs(action_factory->create(), code, args,
                                        file.string(), "optix-index") ;
}

#else

// 无 LibTooling 时返回 false，走兜底文本扫描。
bool scan_source_file_libtooling(const std::filesystem::path&,
                                 const std::filesystem::path&,
                                 optix::core::CodeFactGraph*) {
  return false;
}

#endif

} // namespace

// run 扫描源码目录并生成 CodeFactGraph。
optix::core::Artifact IndexStage::run(const optix::core::ArtifactMap&,
                                      optix::core::Context& ctx) {
  optix::core::CodeFactGraph graph;
  graph.meta = optix::core::make_metadata("stage.index", ctx.run_id);

  if (!std::filesystem::exists(ctx.inputs.source_root)) {
    throw std::runtime_error("source root does not exist: " +
                             ctx.inputs.source_root.string());
  }

  for (auto it = std::filesystem::recursive_directory_iterator(ctx.inputs.source_root);
       it != std::filesystem::recursive_directory_iterator(); ++it) {
    if (!it->is_regular_file()) {
      continue;
    }
    if (!is_cpp_file(it->path())) {
      continue;
    }

    const auto function_count_before = graph.functions.size();
    const bool tooling_ok =
        scan_source_file_libtooling(it->path(), ctx.inputs.compile_commands, &graph);

    // LibTooling 不可用/失败/未产生函数事实时，回退文本扫描保证可用性。
    if (!tooling_ok || graph.functions.size() == function_count_before) {
      scan_source_file_text_fallback(it->path(), &graph);
    }
  }

  optix::core::Artifact out;
  out.stage_id = name();
  out.meta = optix::core::make_metadata("stage.index", ctx.run_id);
  out.data = std::move(graph);
  return out;
}

} // namespace optix::stages
