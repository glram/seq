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
  Type(bool callable, std::string name);
  Type(std::shared_ptr<Type> rType, std::vector<std::shared_ptr<Type>> argTypes,
       std::string name);
  Type(std::vector<std::shared_ptr<Type>> members,
       std::vector<std::string> names, std::string name);

  bool isCallable();

  std::shared_ptr<Type> getMemberType(std::string name);
  std::string textRepresentation() const;
  std::string getName();

  std::shared_ptr<Type> getRType();
  std::vector<std::shared_ptr<Type>> getArgTypes();

  std::shared_ptr<Func> findMagic(std::string name,
                                  std::vector<std::shared_ptr<Type>> types);

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
extern auto kStringType = std::make_shared<Type>(false, "str");
extern auto kBoolType = std::make_shared<Type>(false, "bool");
extern auto kSeqType = std::make_shared<Type>(false, "seq");
extern auto kFloatType = std::make_shared<Type>(false, "float");
extern auto kIntType = std::make_shared<Type>(false, "int");
extern auto kAnyType = std::make_shared<Type>(false, "any");
extern auto kVoidType = std::make_shared<Type>(false, "void");
extern auto kByteType = std::make_shared<Type>(false, "byte");

extern auto kNoArgVoidFuncType = std::make_shared<Type>(
    kVoidType, std::vector<std::shared_ptr<Type>>(), "void->void");
} // namespace types
} // namespace ir
} // namespace seq
