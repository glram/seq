#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "sir/base.h"
#include "sir/codegen/codegen.h"

namespace seq {
namespace ir {

class Func;

namespace types {

class Type;

extern std::shared_ptr<Type> kStringType;
extern std::shared_ptr<Type> kBoolType;
extern std::shared_ptr<Type> kSeqType;
extern std::shared_ptr<Type> kFloatType;
extern std::shared_ptr<Type> kIntType;
extern std::shared_ptr<Type> kAnyType;
extern std::shared_ptr<Type> kVoidType;
extern std::shared_ptr<Type> kByteType;

extern std::shared_ptr<Type> kNoArgVoidFuncType;

class Type : public AttributeHolder<Type> {
private:
  static int currentId;

  std::string name;
  int id;

  bool atomic;

public:
  explicit Type(std::string name, bool atomic = false)
      : name(std::move(name)), id(currentId++), atomic(atomic) {}
  virtual ~Type() = default;

  virtual void accept(seq::ir::codegen::CodegenVisitor &v) { v.visit(getShared()); }

  static void resetId();

  std::string getName() const { return name; }

  int getId() const { return id; }

  virtual bool isRef() const { return false; }
  bool isAtomic() const { return atomic; }

  std::string referenceString() const override;
  virtual std::string textRepresentation() const override { return referenceString(); }
};

class MemberedType : public Type {
public:
  explicit MemberedType(std::string name) : Type(std::move(name)) {}

  virtual std::vector<std::string> getMemberNames() = 0;
  virtual std::vector<std::shared_ptr<Type>> getMemberTypes() = 0;
  virtual std::shared_ptr<Type> getMemberType(std::string n) = 0;
};

class RecordType : public MemberedType {
private:
  std::vector<std::string> memberNames;
  std::vector<std::shared_ptr<Type>> memberTypes;

public:
  RecordType(std::string name, std::vector<std::shared_ptr<Type>> mTypes,
             std::vector<std::string> mNames)
      : MemberedType(std::move(name)), memberNames(std::move(mNames)),
        memberTypes(std::move(mTypes)) {}

  void accept(seq::ir::codegen::CodegenVisitor &v) override {
    v.visit(std::static_pointer_cast<RecordType>(getShared()));
  }

  std::vector<std::string> getMemberNames() override { return memberNames; }
  std::vector<std::shared_ptr<Type>> getMemberTypes() override { return memberTypes; }
  std::shared_ptr<Type> getMemberType(std::string n) override;

  std::string textRepresentation() const override;
};

class RefType : public MemberedType {
private:
  std::shared_ptr<RecordType> contents;

public:
  RefType(std::string name, std::shared_ptr<RecordType> contents)
      : MemberedType(name), contents(std::move(contents)) {}

  void accept(seq::ir::codegen::CodegenVisitor &v) override {
    v.visit(std::static_pointer_cast<RefType>(getShared()));
  }

  std::shared_ptr<RecordType> getContents() { return contents; }
  std::vector<std::string> getMemberNames() override {
    return contents->getMemberNames();
  }
  std::vector<std::shared_ptr<Type>> getMemberTypes() override {
    return contents->getMemberTypes();
  }
  std::shared_ptr<Type> getMemberType(std::string n) override {
    return contents->getMemberType(n);
  }

  bool isRef() const override { return true; }

  std::string textRepresentation() const override;
};

class FuncType : public Type {
private:
  std::shared_ptr<Type> rType;
  std::vector<std::shared_ptr<Type>> argTypes;

public:
  FuncType(std::string name, std::shared_ptr<Type> rType,
           std::vector<std::shared_ptr<Type>> argTypes)
      : Type(std::move(name)), rType(std::move(rType)), argTypes(std::move(argTypes)) {}

  void accept(seq::ir::codegen::CodegenVisitor &v) override {
    v.visit(std::static_pointer_cast<FuncType>(getShared()));
  }

  std::shared_ptr<Type> getRType() { return rType; }
  std::vector<std::shared_ptr<Type>> getArgTypes() { return argTypes; }

  std::string textRepresentation() const override;
};

class PartialFuncType : public FuncType {
private:
  std::shared_ptr<Type> callee;
  std::vector<std::shared_ptr<Type>> callTypes;

public:
  PartialFuncType(std::string name, std::shared_ptr<FuncType> callee,
                  std::vector<std::shared_ptr<Type>> callTypes);

  void accept(seq::ir::codegen::CodegenVisitor &v) override {
    v.visit(std::static_pointer_cast<PartialFuncType>(getShared()));
  }

  std::shared_ptr<Type> getCallee() { return callee; }
  std::vector<std::shared_ptr<Type>> getCallTypes() { return callTypes; };

  std::string textRepresentation() const override;
};

class Optional : public Type {
private:
  std::shared_ptr<Type> base;

public:
  explicit Optional(std::shared_ptr<Type> base);

  void accept(seq::ir::codegen::CodegenVisitor &v) override {
    v.visit(std::static_pointer_cast<Optional>(getShared()));
  }

  std::shared_ptr<Type> getBase() { return base; }
};

class Array : public Type {
private:
  std::shared_ptr<Type> base;

public:
  explicit Array(std::shared_ptr<Type> base);

  void accept(seq::ir::codegen::CodegenVisitor &v) override {
    v.visit(std::static_pointer_cast<Array>(getShared()));
  }

  std::shared_ptr<Type> getBase() { return base; }
};

class Pointer : public Type {
private:
  std::shared_ptr<Type> base;

public:
  explicit Pointer(std::shared_ptr<Type> base);

  void accept(seq::ir::codegen::CodegenVisitor &v) override {
    v.visit(std::static_pointer_cast<Pointer>(getShared()));
  }

  std::shared_ptr<Type> getBase() { return base; }
};

class Generator : public Type {
private:
  std::shared_ptr<Type> base;

public:
  explicit Generator(std::shared_ptr<Type> base);

  void accept(seq::ir::codegen::CodegenVisitor &v) override {
    v.visit(std::static_pointer_cast<Generator>(getShared()));
  }

  std::shared_ptr<Type> getBase() { return base; }
};

class IntNType : public Type {
private:
  unsigned len;
  bool sign;

public:
  static const unsigned MAX_LEN = 2048;

  IntNType(unsigned len, bool sign);

  void accept(seq::ir::codegen::CodegenVisitor &v) override {
    v.visit(std::static_pointer_cast<IntNType>(getShared()));
  }

  std::string oppositeSignName() const;

  unsigned getLen() const { return len; }
  bool isSigned() const { return sign; }
};

} // namespace types
} // namespace ir
} // namespace seq
