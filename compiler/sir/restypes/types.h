#pragma once

#include <memory>
#include <vector>

#include "../base.h"

namespace seq {
namespace ir {
namespace restypes {

// TODO functors/type names

class Type : AttributeHolder {
private:
  bool callable;
  std::string name;
public:
  Type(bool callable, std::string name);
  bool isCallable();

  std::shared_ptr<Type> getMemberType(std::string name);
  std::string textRepresentation() const;
  std::string getName();
};

class FuncType : Type {
private:
  std::shared_ptr<Type> rType;
  std::vector<std::shared_ptr<Type>> argTypes;

public:
  FuncType(std::shared_ptr<Type> rType,
           std::vector<std::shared_ptr<Type>> argTypes);

  std::shared_ptr<Type> getRType();
  std::vector<std::shared_ptr<Type>> getArgTypes();
};

class LiteralType : Type {
private:
  std::string type;

public:
  LiteralType(std::string type);
};

// TODO better system
static auto kStringType = std::make_shared<LiteralType>("str");
static auto kBoolType = std::make_shared<LiteralType>("bool");
static auto kSeqType = std::make_shared<LiteralType>("seq");
static auto kDoubleType = std::make_shared<LiteralType>("double");
static auto kIntType = std::make_shared<LiteralType>("int");
static auto kAnyType = std::make_shared<Type>(false, "any");
} // namespace restypes
} // namespace ir
} // namespace seq