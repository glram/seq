#pragma once

#include <memory>
#include <string>
#include <vector>

#include "base.h"

#include "codegen/codegen.h"

namespace seq {
namespace ir {

class Var;
class Func;
class TryCatch;

class IRModule : public AttributeHolder<IRModule> {
private:
  std::vector<std::shared_ptr<Var>> globals;
  std::string name;
  std::shared_ptr<Func> baseFunc;
  std::shared_ptr<Var> argVar;
  std::shared_ptr<TryCatch> tc;

public:
  explicit IRModule(std::string name);

  void accept(codegen::CodegenVisitor &v) { v.visit(getShared()); }

  std::vector<std::shared_ptr<Var>> getGlobals() { return globals; }
  void addGlobal(std::shared_ptr<Var> var);

  std::shared_ptr<Func> getBase() const { return baseFunc; }

  std::string getName() const { return name; }

  std::shared_ptr<Var> getArgVar() { return argVar; };

  std::shared_ptr<TryCatch> getTryCatch() { return tc; }

  std::string referenceString() const override { return "module"; };
  std::string textRepresentation() const override;
};

} // namespace ir
} // namespace seq
