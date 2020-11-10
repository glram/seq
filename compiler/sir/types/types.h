#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "sir/base.h"

namespace seq {
namespace ir {

namespace common {
class SIRVisitor;
}

class Func;

namespace types {

class Type;

extern std::shared_ptr<Type> kBoolType;
extern std::shared_ptr<Type> kFloatType;
extern std::shared_ptr<Type> kIntType;
extern std::shared_ptr<Type> kAnyType;
extern std::shared_ptr<Type> kVoidType;
extern std::shared_ptr<Type> kByteType;

extern std::shared_ptr<Type> kBytePointerType;

extern std::shared_ptr<Type> kStringType;
extern std::shared_ptr<Type> kSeqType;

extern std::shared_ptr<Type> kStringPointerType;
extern std::shared_ptr<Type> kStringArrayType;

extern std::shared_ptr<Type> kNoArgVoidFuncType;

/// Type from which other SIR types derive.
class Type : public AttributeHolder<Type> {
private:
  /// globally shared type counter (types have unique ids).
  static int currentId;

  /// name of the type, empty if unnamed
  std::string name;

  /// id of the type, 0 is reserved for void
  int id;

  /// true if atomic
  bool atomic;

public:
  /// Constructs a type.
  /// @param name the type's name
  /// @param atomic atomicity of the type
  explicit Type(std::string name, bool atomic = false)
      : name(std::move(name)), id(currentId++), atomic(atomic) {}

  virtual ~Type() = default;
  virtual void accept(common::SIRVisitor &v);

  /// Resets the shared id counter. Should only be used in testing.
  static void resetId();

  /// @return the type's name
  std::string getName() const { return name; }

  /// @return the type's unique id
  int getId() const { return id; }

  /// @return true if the type is a reference type, false otherwise
  virtual bool isRef() const { return false; }

  /// @return true if the type is atomic, false otherwise
  bool isAtomic() const { return atomic; }

  std::string referenceString() const override;
  std::string textRepresentation() const override { return referenceString(); }
};

/// Type from which membered (field-containing) SIR types derive.
class MemberedType : public Type {
public:
  /// Constructs a membered type.
  /// @param name the type's name
  explicit MemberedType(std::string name) : Type(std::move(name)) {}

  /// @return the type's members' names
  virtual std::vector<std::string> getMemberNames() = 0;

  /// @return the type's members' types
  virtual std::vector<std::shared_ptr<Type>> getMemberTypes() = 0;

  /// Returns the type of a specific member.
  /// @param n the field's name
  /// @return the type of the member if it exists, null otherwise
  virtual std::shared_ptr<Type> getMemberType(const std::string &n) = 0;
};

/// Membered type equivalent to C structs/C++ PODs
class RecordType : public MemberedType {
private:
  /// the member names
  std::vector<std::string> memberNames;

  /// the member types
  std::vector<std::shared_ptr<Type>> memberTypes;

public:
  /// Constructs a record type.
  /// @param name the type's name
  /// @param mTypes the member types
  /// @param mNames the member names
  RecordType(std::string name, std::vector<std::shared_ptr<Type>> mTypes,
             std::vector<std::string> mNames)
      : MemberedType(std::move(name)), memberNames(std::move(mNames)),
        memberTypes(std::move(mTypes)) {}

  /// Constructs a record type. The field's names are "1", "2"...
  /// @param name the type's name
  /// @param mTypes a vector of member types
  RecordType(std::string name, std::vector<std::shared_ptr<Type>> mTypes);

  void accept(common::SIRVisitor &v) override;

  std::vector<std::string> getMemberNames() override { return memberNames; }

  /// Sets the member names.
  /// @param a vector of member names
  void setMemberNames(std::vector<std::string> names) {
    memberNames = std::move(names);
  }

  std::vector<std::shared_ptr<Type>> getMemberTypes() override { return memberTypes; }

  /// Sets the member types
  /// @param types a vector of member types
  void setMemberTypes(std::vector<std::shared_ptr<Type>> types) {
    memberTypes = std::move(types);
  }

  std::shared_ptr<Type> getMemberType(const std::string &n) override;

  std::string textRepresentation() const override;
};

/// Membered type that is passed by reference. Similar to Python classes.
class RefType : public MemberedType {
private:
  /// the internal contents of the type
  std::shared_ptr<RecordType> contents;

public:
  /// Constructs a reference type.
  /// @param name the type's name
  /// @param contents the type's contents
  RefType(std::string name, std::shared_ptr<RecordType> contents)
      : MemberedType(std::move(name)), contents(std::move(contents)) {}

  void accept(common::SIRVisitor &v) override;

  /// @return the record's contents
  std::shared_ptr<RecordType> getContents() { return contents; }

  /// Set the record's contents
  /// @param c the new contents
  void setContents(std::shared_ptr<RecordType> c) { contents = std::move(c); }

  std::vector<std::string> getMemberNames() override {
    return contents->getMemberNames();
  }

  std::vector<std::shared_ptr<Type>> getMemberTypes() override {
    return contents->getMemberTypes();
  }

  std::shared_ptr<Type> getMemberType(const std::string &n) override {
    return contents->getMemberType(n);
  }

  bool isRef() const override { return true; }

  std::string textRepresentation() const override;
};

/// Type associated with a SIR function.
class FuncType : public Type {
private:
  /// Return type
  std::shared_ptr<Type> rType;

  /// Argument types
  std::vector<std::shared_ptr<Type>> argTypes;

public:
  /// Constructs a function type.
  /// @param name the type's name
  /// @param rType the function's return type
  /// @param argTypes the function's arg types
  FuncType(std::string name, std::shared_ptr<Type> rType,
           std::vector<std::shared_ptr<Type>> argTypes)
      : Type(std::move(name)), rType(std::move(rType)), argTypes(std::move(argTypes)) {}

  void accept(common::SIRVisitor &v) override;

  /// @return true if this is a partial function, false otherwise
  virtual bool isPartial() const { return false; }

  /// @return the function's return type
  std::shared_ptr<Type> getRType() { return rType; }

  /// @return the function's arg types
  std::vector<std::shared_ptr<Type>> getArgTypes() { return argTypes; }

  std::string textRepresentation() const override;
};

/// Type associated with a partial function
class PartialFuncType : public FuncType {
private:
  /// Callee function type
  std::shared_ptr<Type> callee;

  /// Supplied arg types, nullptr indicates no arg supplied
  std::vector<std::shared_ptr<Type>> callTypes;

public:
  /// Constructs a partial function type.
  /// @param name the type's name
  /// @param callee the callee function type
  /// @param callTypes
  PartialFuncType(std::string name, std::shared_ptr<FuncType> callee,
                  std::vector<std::shared_ptr<Type>> callTypes);

  void accept(common::SIRVisitor &v) override;

  bool isPartial() const override { return true; }

  /// @return the callee type
  std::shared_ptr<Type> getCallee() { return callee; }

  /// @return the arg types
  std::vector<std::shared_ptr<Type>> getCallTypes() { return callTypes; };

  std::string textRepresentation() const override;
};

/// Type of a pointer to another SIR type
class PointerType : public Type {
private:
  /// type's base type
  std::shared_ptr<Type> base;

public:
  /// Constructs a pointer type.
  /// @param base the type's base
  explicit PointerType(std::shared_ptr<Type> base);

  void accept(common::SIRVisitor &v) override;

  /// @return the base type
  std::shared_ptr<Type> getBase() { return base; }
};

/// Type of an optional containing another SIR type
class OptionalType : public RecordType {
private:
  std::shared_ptr<Type> base;

public:
  /// Constructs an optional type.
  /// @param base the type's base
  explicit OptionalType(std::shared_ptr<PointerType> pointerBase);

  void accept(common::SIRVisitor &v) override;

  /// @return the base type
  std::shared_ptr<Type> getBase() { return base; }
};

/// Type of an array containing another SIR type
class ArrayType : public RecordType {
private:
  std::shared_ptr<Type> base;

public:
  /// Constructs an array type.
  /// @param base the type's base
  explicit ArrayType(std::shared_ptr<PointerType> pointerType);

  void accept(common::SIRVisitor &v) override;

  /// @return the base type
  std::shared_ptr<Type> getBase() { return base; }
};

/// Type of a generator yielding another SIR type
class GeneratorType : public Type {
private:
  std::shared_ptr<Type> base;

public:
  /// Constructs a generator type.
  /// @param base the type's base
  explicit GeneratorType(std::shared_ptr<Type> base);

  void accept(common::SIRVisitor &v) override;

  /// @return the base type
  std::shared_ptr<Type> getBase() { return base; }
};

/// Type of a variably sized integer
class IntNType : public Type {
private:
  /// length of the integer
  unsigned len;
  /// whether the variable is signed
  bool sign;

public:
  static const unsigned MAX_LEN = 2048;

  /// Constructs a variably sized integer type.
  /// @param len the length of the integr
  /// @param sign true if signed, false otherwise
  IntNType(unsigned len, bool sign);

  void accept(common::SIRVisitor &v) override;

  /// @return the name of the opposite signed corresponding type
  std::string oppositeSignName() const;

  /// @return the length of the integer
  unsigned getLen() const { return len; }

  /// @return true if signed, false otherwise
  bool isSigned() const { return sign; }
};

} // namespace types
} // namespace ir
} // namespace seq
