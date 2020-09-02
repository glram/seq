#pragma once

#include <memory>
#include <string>
#include <vector>

#include "../base.h"

namespace seq {
namespace ir {

class Func;

namespace types {

// TODO functors/type names

class Type : public AttributeHolder<Type> {
private:
  bool callable;
  std::string name;
  std::vector<std::string> memberNames;
  std::vector<std::shared_ptr<Type>> memberTypes;
  std::shared_ptr<Type> rType;
  std::vector<std::shared_ptr<Type>> argTypes;

public:
  explicit Type(std::string name) : callable(false), name(std::move(name)) {}
  Type(std::shared_ptr<Type> rType, std::vector<std::shared_ptr<Type>> argTypes,
       std::string name)
      : callable(true), name(std::move(name)), rType(std::move(rType)),
        argTypes(std::move(argTypes)) {}
  Type(std::vector<std::shared_ptr<Type>> members,
       std::vector<std::string> names, std::string name)
      : callable(false), name(std::move(name)), memberNames(std::move(names)),
        memberTypes(std::move(members)) {}

  bool isCallable() const { return callable; }

  std::shared_ptr<Type> getMemberType(std::string name){
      return memberTypes[memberNames.]} std::string
      textRepresentation() const override {
    return name;
  }
  std::string getName() { return name; }

  std::shared_ptr<Type> getRType() { return rType; }
  std::vector<std::shared_ptr<Type>> getArgTypes() { return argTypes; }

  std::string referenceString() const override { return "type"; };
};

class Optional : public Type {
private:
  std::shared_ptr<Type> base;

public:
  explicit Optional(std::shared_ptr<Type> base);
};

class Array : public Type {
private:
  std::shared_ptr<Type> base;

public:
  explicit Array(std::shared_ptr<Type> base);
};

class Pointer : public Type {
private:
  std::shared_ptr<Type> base;

public:
  explicit Pointer(std::shared_ptr<Type> base);
};

class Reference : public Type {
private:
  std::shared_ptr<Type> base;

public:
  explicit Reference(std::shared_ptr<Type> base);
};

class Generator : public Type {
private:
  std::shared_ptr<Type> base;

public:
  explicit Generator(std::shared_ptr<Type> base);
};

// TODO better system
extern const auto kStringType = std::make_shared<Type>("str");
extern const auto kBoolType = std::make_shared<Type>("bool");
extern const auto kSeqType = std::make_shared<Type>("seq");
extern const auto kFloatType = std::make_shared<Type>("float");
extern const auto kIntType = std::make_shared<Type>("int");
extern const auto kAnyType = std::make_shared<Type>("any");
extern const auto kVoidType = std::make_shared<Type>("void");
extern const auto kByteType = std::make_shared<Type>("byte");

extern const auto kNoArgVoidFuncType = std::make_shared<Type>(
    kVoidType, std::vector<std::shared_ptr<Type>>(), "void->void");
} // namespace types
} // namespace ir
} // namespace seq
