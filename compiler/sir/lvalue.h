#pragma once

#include <memory>

#include "base.h"

namespace seq {
namespace ir {

class Var;

class Lvalue : public AttributeHolder<Lvalue> {};

class VarLvalue : public Lvalue {
private:
  std::weak_ptr<Var> var;
};

class VarMemberLvalue : public Lvalue {
private:
  std::weak_ptr<Var> var;
  std::string field;
};

} // namespace ir
} // namespace seq