#pragma once

#include <memory>
#include <vector>

#include "../base.h"

namespace seq {
namespace ir {
namespace types {

// TODO functors/type names

class Type : AttributeHolder<Type> {
private:
  bool callable;
  std::string name;
  std::vector<std::string> memberNames;
  std::vector<std::weak_ptr<Type>> memberTypes;

public:
  Type(bool callable, std::string name);
  bool isCallable();

  std::weak_ptr<Type> getMemberType(std::string name);
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

// TODO better system
static auto kStringType = std::make_shared<Type>(false, "str");
static auto kBoolType = std::make_shared<Type>(false, "bool");
static auto kSeqType = std::make_shared<Type>(false, "seq");
static auto kFloatType = std::make_shared<Type>(false, "float");
static auto kIntType = std::make_shared<Type>(false, "int");
static auto kAnyType = std::make_shared<Type>(false, "any");
static auto kVoidType = std::make_shared<Type>(false, "void");
static auto kByteType = std::make_shared<Type>(false, "byte");

static auto kNoArgVoidFuncType =
    std::make_shared<FuncType>(kVoidType, std::vector<std::shared_ptr<Type>>());
} // namespace types
} // namespace ir
} // namespace seq