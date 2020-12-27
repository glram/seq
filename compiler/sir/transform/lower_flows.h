#pragma once

#include "sir/util/context.h"
#include "sir/util/visitor.h"

#include "sir/sir.h"

namespace seq {
namespace ir {
namespace transform {

class LowerFlowsVisitor : public util::SIRVisitor {
private:
struct Context : public util::SIRContext<SeriesFlow> {
  Func *curFunc = nullptr;
} ctx;

public:
  void visit(IRModule *module) override;

};

} // namespace transform
} // namespace ir
} // namespace seq
