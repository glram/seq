#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "lang/seq.h"
#include "parser/ast/cache.h"
#include "parser/ast/codegen/codegen.h"
#include "parser/ast/format/format.h"
#include "parser/ast/transform/transform.h"
#include "parser/ast/typecheck/typecheck.h"
#include "parser/ocaml.h"
#include "parser/parser.h"
#include "util/fmt/format.h"

#include "sir/codegen/util.h"
#include "sir/module.h"

using std::make_shared;
using std::string;
using std::vector;

int __level__ = 0;
int __dbg_level__ = 0;
bool __isTest = false;

namespace seq {

void generateDocstr(const std::string &file) {
  // LOG("DOC MODE! {}", 1);
  // ast::DocStmtVisitor d;
  // ast::parse_file(file)->accept(d);
}

std::shared_ptr<ir::IRModule> parse(const std::string &argv0, const std::string &file,
                                    const string &code, bool isCode, bool isTest,
                                    int startLine) {
  try {
    auto d = getenv("SEQ_DEBUG");
    if (d)
      __dbg_level__ = strtol(d, nullptr, 10);

    char abs[PATH_MAX + 1];
    realpath(file.c_str(), abs);

    ast::StmtPtr codeStmt =
        isCode ? ast::parseCode(abs, code, startLine) : ast::parseFile(abs);

    using namespace std::chrono;

    auto cache = make_shared<ast::Cache>(argv0);

    auto t = high_resolution_clock::now();
    auto transformed = ast::TransformVisitor::apply(cache, move(codeStmt));
    FILE *fo;
    if (!isTest) {
      fmt::print(stderr, "[T] transform = {:.1f}\n",
                 duration_cast<milliseconds>(high_resolution_clock::now() - t).count() /
                     1000.0);
      fo = fopen("_dump.seq", "w");
      fmt::print(fo, "=== Transform ===\n{}\n", ast::FormatVisitor::apply(transformed));
      fflush(fo);
    }

    t = high_resolution_clock::now();
    auto typechecked = ast::TypecheckVisitor::apply(cache, move(transformed));
    if (!isTest) {
      fmt::print(stderr, "[T] typecheck = {:.1f}\n",
                 duration_cast<milliseconds>(high_resolution_clock::now() - t).count() /
                     1000.0);
      fmt::print(fo, "=== Typecheck ===\n{}\n",
                 ast::FormatVisitor::apply(typechecked, cache));
      fflush(fo);
    }
    // FILE *fo = fopen("tmp/out.htm", "w");
    // LOG3("{}", ast::FormatVisitor::format(ctx, tv, false, true));

    t = high_resolution_clock::now();
    auto module = ast::CodegenVisitor::apply(cache, move(typechecked));
    if (!isTest) {
      fmt::print(stderr, "[T] codegen   = {:.1f}\n",
                 duration_cast<milliseconds>(high_resolution_clock::now() - t).count() /
                     1000.0);
    }
    __isTest = isTest;

    return module;
  } catch (seq::exc::SeqException &e) {
    if (isTest) {
      LOG("ERROR: {}", e.what());
    } else {
      seq::compilationError(e.what(), e.getSrcInfo().file, e.getSrcInfo().line,
                            e.getSrcInfo().col);
    }
    exit(EXIT_FAILURE);
    return nullptr;
  } catch (seq::exc::ParserException &e) {
    for (int i = 0; i < e.messages.size(); i++) {
      if (isTest) {
        LOG("ERROR: {}", e.messages[i]);
      } else {
        compilationMessage("\033[1;31merror:\033[0m", e.messages[i],
                           e.locations[i].file, e.locations[i].line,
                           e.locations[i].col);
      }
    }
    exit(EXIT_FAILURE);
    return nullptr;
  }
}

// void execute(seq::SeqModule *module, vector<string> args, vector<string> libs,
//             bool debug) {
//  config::config().debug = debug;
//  // try {
//  module->execute(args, libs, !__isTest);
//  // } catch (exc::SeqException &e) {
//  // compilationError(e.what(), e.getSrcInfo().file, e.getSrcInfo().line,
//  //  e.getSrcInfo().col);
//  // }
//}

void compile(std::shared_ptr<ir::IRModule> module, const string &out, bool debug) {
  using namespace std::chrono;
  auto t = high_resolution_clock::now();

  config::config().debug = debug;
  try {

    llvm::LLVMContext context;
    auto *llvmModule = seq::ir::codegen::compile(context, module);
    std::error_code err;
    llvm::raw_fd_ostream stream(out, err, llvm::sys::fs::F_None);

#if LLVM_VERSION_MAJOR >= 7
    WriteBitcodeToFile(*module, stream);
#else
    llvm::WriteBitcodeToFile(llvmModule, stream);
#endif

    fmt::print(stderr, "[T] compile   = {:.1f}\n",
               duration_cast<milliseconds>(high_resolution_clock::now() - t).count() /
                   1000.0);
    if (err) {
      std::cerr << "error: " << err.message() << std::endl;
      exit(err.value());
    }
  } catch (exc::SeqException &e) {
    compilationError(e.what(), e.getSrcInfo().file, e.getSrcInfo().line,
                     e.getSrcInfo().col);
  }
}

} // namespace seq
