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
  std::string name;

public:
  Type(std::string name)
      : name(std::move(name)) {}
  virtual ~Type() = default;

  std::string getName() const { return name; }

  std::string referenceString() const override { return name; };
  virtual std::string textRepresentation() const override { return name; }
};

class RecordType : public Type {
private:
  std::vector<std::string> memberNames;
  std::vector<std::shared_ptr<Type>> memberTypes;

public:
  RecordType(std::string name, std::vector<std::shared_ptr<Type>> mTypes,
         std::vector<std::string> mNames)
      : Type(name), memberNames(std::move(mNames)), memberTypes(std::move(mTypes)) {}

  std::vector<std::string> getMemberNames() { return memberNames; }

  std::vector<std::shared_ptr<Type>> getMemberTypes() { return memberTypes; }
  std::shared_ptr<Type> getMemberType(std::string n);

  std::string textRepresentation() const override;
};

class FuncType : public Type {
private:
  std::shared_ptr<Type> rType;
  std::vector<std::shared_ptr<Type>> argTypes;

public:
  FuncType(std::string name, std::shared_ptr<Type> rType, std::vector<std::shared_ptr<Type>> argTypes)
      : Type(name), rType(rType), argTypes(argTypes) {}

  std::shared_ptr<Type> getRType() { return rType; }
  std::vector<std::shared_ptr<Type>> getArgTypes() { return argTypes; }

  std::string textRepresentation() const override;
};

class PartialFuncType : public Type {
private:
  std::shared_ptr<Type> callee;
  std::vector<std::shared_ptr<Type>> callTypes;

public:
  PartialFuncType(std::string name, std::shared_ptr<Type> callee, std::vector<std::shared_ptr<Type>> callTypes)
  : Type(name), callee(std::move(callee)), callTypes(std::move(callTypes)) {}

  std::shared_ptr<Type> getCallee() { return callee; }
  std::vector<std::shared_ptr<Type>> getCallTypes() { return callTypes; };

  std::string textRepresentation() const override;
};

class Optional : public Type {
private:
  std::shared_ptr<Type> base;

public:
  explicit Optional(std::shared_ptr<Type> base);

  std::shared_ptr<Type> getBase() { return base; }
};

class Array : public Type {
private:
  std::shared_ptr<Type> base;

public:
  explicit Array(std::shared_ptr<Type> base);

  std::shared_ptr<Type> getBase() { return base; }
};

class Pointer : public Type {
private:
  std::shared_ptr<Type> base;

public:
  explicit Pointer(std::shared_ptr<Type> base);

  std::shared_ptr<Type> getBase() { return base; }
};

class Reference : public Type {
private:
  std::shared_ptr<Type> base;

public:
  explicit Reference(std::shared_ptr<Type> base);

  std::shared_ptr<Type> getBase() { return base; }
};

class Generator : public Type {
private:
  std::shared_ptr<Type> base;

public:
  explicit Generator(std::shared_ptr<Type> base);

  std::shared_ptr<Type> getBase() { return base; }
};

// TODO better system
extern const std::shared_ptr<Type> kStringType;
extern const std::shared_ptr<Type> kBoolType;
extern const std::shared_ptr<Type> kSeqType;
extern const std::shared_ptr<Type> kFloatType;
extern const std::shared_ptr<Type> kIntType;
extern const std::shared_ptr<Type> kUIntType;
extern const std::shared_ptr<Type> kAnyType;
extern const std::shared_ptr<Type> kVoidType;
extern const std::shared_ptr<Type> kByteType;

extern const std::shared_ptr<Type> kNoArgVoidFuncType;
} // namespace types
} // namespace ir
} // namespace seq
