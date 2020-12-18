/*
 * expr.h --- Seq AST expressions.
 *
 * (c) Seq project. All rights reserved.
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 */

#pragma once

#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "lang/seq.h"
#include "parser/ast/types.h"
#include "parser/common.h"

namespace seq {
namespace ast {

// Forward declarations
struct ASTVisitor;
struct BinaryExpr;
struct CallExpr;
struct DotExpr;
struct EllipsisExpr;
struct IdExpr;
struct IfExpr;
struct IndexExpr;
struct IntExpr;
struct ListExpr;
struct NoneExpr;
struct StarExpr;
struct StaticExpr;
struct StringExpr;
struct TupleExpr;
struct UnaryExpr;
struct Stmt;

/**
 * A Seq AST expression.
 * Each AST expression owns its children and is intended to be instantiated as a
 * unique_ptr.
 */
struct Expr : public seq::SrcObject {
private:
  /// Type of the expression. nullptr by default.
  types::TypePtr type;

  /// Flag that indicates if an expression describes a type (e.g. int or list[T]).
  /// Used by transformation and type-checking stages.
  bool isTypeExpr;

public:
  Expr();
  Expr(const Expr &expr) = default;

  /// Convert a node to an S-expression.
  virtual string toString() const = 0;
  /// Deep copy a node.
  virtual unique_ptr<Expr> clone() const = 0;
  /// Accept an AST visitor.
  virtual void accept(ASTVisitor &visitor) const = 0;

  /// Get a node type.
  /// @return Type pointer or a nullptr if a type is not set.
  types::TypePtr getType() const;
  /// Set a node type.
  void setType(types::TypePtr type);
  /// @return true if a node describes a type expression.
  bool isType() const;
  /// Marks a node as a type expression.
  void markType();

  /// Allow pretty-printing to C++ streams.
  friend std::ostream &operator<<(std::ostream &out, const Expr &expr) {
    return out << expr.toString();
  }

  /// Convenience virtual functions to avoid unnecessary dynamic_cast calls.
  virtual bool isId(string &&val) const { return false; }
  virtual const BinaryExpr *getBinary() const { return nullptr; }
  virtual const CallExpr *getCall() const { return nullptr; }
  virtual const DotExpr *getDot() const { return nullptr; }
  virtual const EllipsisExpr *getEllipsis() const { return nullptr; }
  virtual const IdExpr *getId() const { return nullptr; }
  virtual const IfExpr *getIf() const { return nullptr; }
  virtual const IndexExpr *getIndex() const { return nullptr; }
  virtual const IntExpr *getInt() const { return nullptr; }
  virtual const ListExpr *getList() const { return nullptr; }
  virtual const NoneExpr *getNone() const { return nullptr; }
  virtual const StarExpr *getStar() const { return nullptr; }
  virtual const StaticExpr *getStatic() const { return nullptr; }
  virtual const StringExpr *getString() const { return nullptr; }
  virtual const TupleExpr *getTuple() const { return nullptr; }
  virtual const UnaryExpr *getUnary() const { return nullptr; }

protected:
  /// Add a type to S-expression string.
  string wrapType(const string &sexpr) const;
};
using ExprPtr = unique_ptr<Expr>;

/// Function signature parameter helper node (name: type = deflt).
struct Param {
  string name;
  ExprPtr type;
  ExprPtr deflt;

  explicit Param(string name = "", ExprPtr type = nullptr, ExprPtr deflt = nullptr);

  string toString() const;
  Param clone() const;
};

/// None expression.
/// @example None
struct NoneExpr : public Expr {
  NoneExpr();
  NoneExpr(const NoneExpr &expr) = default;

  string toString() const override;
  ExprPtr clone() const override;
  void accept(ASTVisitor &visitor) const override;

  const NoneExpr *getNone() const override { return this; }
};

/// Bool expression (value).
/// @example True
struct BoolExpr : public Expr {
  bool value;

  explicit BoolExpr(bool value);
  BoolExpr(const BoolExpr &expr) = default;

  string toString() const override;
  ExprPtr clone() const override;
  void accept(ASTVisitor &visitor) const override;
};

/// Int expression (value.suffix).
/// @example 12
/// @example 13u
/// @example 000_010b
struct IntExpr : public Expr {
  /// Expression value is stored as a string that is parsed during the simplify stage.
  string value;
  /// Number suffix (e.g. "u" for "123u").
  string suffix;

  /// Parsed value and sign for "normal" 64-bit integers.
  int64_t intValue;

  explicit IntExpr(long long intValue);
  explicit IntExpr(const string &value, string suffix = "");
  IntExpr(const IntExpr &expr) = default;

  string toString() const override;
  ExprPtr clone() const override;
  void accept(ASTVisitor &visitor) const override;

  const IntExpr *getInt() const override { return this; }
};

/// Float expression (value.suffix).
/// @example 12.1
/// @example 13.15z
/// @example e-12
struct FloatExpr : public Expr {
  double value;
  /// Number suffix (e.g. "u" for "123u").
  string suffix;

  explicit FloatExpr(double value, string suffix = "");
  FloatExpr(const FloatExpr &expr) = default;

  string toString() const override;
  ExprPtr clone() const override;
  void accept(ASTVisitor &visitor) const override;
};

/// String expression (prefix"value").
/// @example s'ACGT'
/// @example "fff"
struct StringExpr : public Expr {
  string value;
  string prefix;

  explicit StringExpr(string value, string prefix = "");
  StringExpr(const StringExpr &expr) = default;

  string toString() const override;
  ExprPtr clone() const override;
  void accept(ASTVisitor &visitor) const override;

  const StringExpr *getString() const override { return this; }
};

/// Identifier expression (value).
struct IdExpr : public Expr {
  string value;

  explicit IdExpr(string value);
  IdExpr(const IdExpr &expr) = default;

  string toString() const override;
  ExprPtr clone() const override;
  void accept(ASTVisitor &visitor) const override;

  bool isId(string &&val) const override { return this->value == val; }
  const IdExpr *getId() const override { return this; }
};

/// Star (unpacking) expression (*what).
/// @example *args
struct StarExpr : public Expr {
  ExprPtr what;

  explicit StarExpr(ExprPtr what);
  StarExpr(const StarExpr &expr);

  string toString() const override;
  ExprPtr clone() const override;
  void accept(ASTVisitor &visitor) const override;

  const StarExpr *getStar() const override { return this; }
};

/// Tuple expression ((items...)).
/// @example (1, a)
struct TupleExpr : public Expr {
  vector<ExprPtr> items;

  explicit TupleExpr(vector<ExprPtr> &&items);
  TupleExpr(const TupleExpr &expr);

  string toString() const override;
  ExprPtr clone() const override;
  void accept(ASTVisitor &visitor) const override;

  const TupleExpr *getTuple() const override { return this; }
};

/// List expression ([items...]).
/// @example [1, 2]
struct ListExpr : public Expr {
  vector<ExprPtr> items;

  explicit ListExpr(vector<ExprPtr> &&items);
  ListExpr(const ListExpr &expr);

  string toString() const override;
  ExprPtr clone() const override;
  void accept(ASTVisitor &visitor) const override;

  const ListExpr *getList() const override { return this; }
};

/// Set expression ({items...}).
/// @example {1, 2}
struct SetExpr : public Expr {
  vector<ExprPtr> items;

  explicit SetExpr(vector<ExprPtr> &&items);
  SetExpr(const SetExpr &expr);

  string toString() const override;
  ExprPtr clone() const override;
  void accept(ASTVisitor &visitor) const override;
};

/// Dictionary expression ({(key: value)...}).
/// @example {'s': 1, 't': 2}
struct DictExpr : public Expr {
  struct DictItem {
    ExprPtr key, value;

    DictItem clone() const;
  };
  vector<DictItem> items;

  explicit DictExpr(vector<DictItem> &&items);
  DictExpr(const DictExpr &expr);

  string toString() const override;
  ExprPtr clone() const override;
  void accept(ASTVisitor &visitor) const override;
};

/// Generator body node helper [for vars in gen (if conds)...].
/// @example for i in lst if a if b
struct GeneratorBody {
  ExprPtr vars;
  ExprPtr gen;
  vector<ExprPtr> conds;

  GeneratorBody clone() const;
};

/// Generator or comprehension expression [(expr (loops...))].
/// @example [i for i in j]
/// @example (f + 1 for j in k if j for f in j)
struct GeneratorExpr : public Expr {
  /// Generator kind: normal generator, list comprehension, set comprehension.
  enum GeneratorKind { Generator, ListGenerator, SetGenerator };

  GeneratorKind kind;
  ExprPtr expr;
  vector<GeneratorBody> loops;

  GeneratorExpr(GeneratorKind kind, ExprPtr expr, vector<GeneratorBody> &&loops);
  GeneratorExpr(const GeneratorExpr &expr);

  string toString() const override;
  ExprPtr clone() const override;
  void accept(ASTVisitor &visitor) const override;
};

/// Dictionary comprehension expression [{key: expr (loops...)}].
/// @example {i: j for i, j in z.items()}
struct DictGeneratorExpr : public Expr {
  ExprPtr key, expr;
  vector<GeneratorBody> loops;

  DictGeneratorExpr(ExprPtr key, ExprPtr expr, vector<GeneratorBody> &&loops);
  DictGeneratorExpr(const DictGeneratorExpr &expr);

  string toString() const override;
  ExprPtr clone() const override;
  void accept(ASTVisitor &visitor) const override;
};

/// Conditional expression [cond if ifexpr else elsexpr].
/// @example 1 if a else 2
struct IfExpr : public Expr {
  ExprPtr cond, ifexpr, elsexpr;

  IfExpr(ExprPtr cond, ExprPtr ifexpr, ExprPtr elsexpr);
  IfExpr(const IfExpr &expr);

  string toString() const override;
  ExprPtr clone() const override;
  void accept(ASTVisitor &visitor) const override;

  const IfExpr *getIf() const override { return this; }
};

/// Unary expression [op expr].
/// @example -56
struct UnaryExpr : public Expr {
  string op;
  ExprPtr expr;

  UnaryExpr(string op, ExprPtr expr);
  UnaryExpr(const UnaryExpr &expr);

  string toString() const override;
  ExprPtr clone() const override;
  void accept(ASTVisitor &visitor) const override;

  const UnaryExpr *getUnary() const override { return this; }
};

/// Binary expression [lexpr op rexpr].
/// @example 1 + 2
/// @example 3 or 4
struct BinaryExpr : public Expr {
  string op;
  ExprPtr lexpr, rexpr;

  /// True if an expression modifies lhs in-place (e.g. a += b).
  bool inPlace;

  BinaryExpr(ExprPtr lexpr, string op, ExprPtr rexpr, bool inPlace = false);
  BinaryExpr(const BinaryExpr &expr);

  string toString() const override;
  ExprPtr clone() const override;
  void accept(ASTVisitor &visitor) const override;

  const BinaryExpr *getBinary() const override { return this; }
};

/// Pipe expression [(op expr)...].
/// op is either "" (only the first item), "|>" or "||>".
/// @example a |> b ||> c
struct PipeExpr : public Expr {
  struct Pipe {
    string op;
    ExprPtr expr;

    Pipe clone() const;
  };

  vector<Pipe> items;
  /// Output type of a "prefix" pipe ending at the index position.
  /// Example: for a |> b |> c, inTypes[1] is typeof(a |> b).
  vector<types::TypePtr> inTypes;

  explicit PipeExpr(vector<Pipe> &&items);
  PipeExpr(const PipeExpr &expr);

  string toString() const override;
  ExprPtr clone() const override;
  void accept(ASTVisitor &visitor) const override;
};

/// Index expression (expr[index]).
/// @example a[5]
struct IndexExpr : public Expr {
  ExprPtr expr, index;

  IndexExpr(ExprPtr expr, ExprPtr index);
  IndexExpr(const IndexExpr &expr);

  string toString() const override;
  ExprPtr clone() const override;
  void accept(ASTVisitor &visitor) const override;

  const IndexExpr *getIndex() const override { return this; }
};

/// Call expression (expr((name=value)...)).
/// @example a(1, b=2)
struct CallExpr : public Expr {
  /// Each argument can have a name (e.g. foo(1, b=5))
  struct Arg {
    string name;
    ExprPtr value;

    Arg clone() const;
  };

  ExprPtr expr;
  vector<Arg> args;

  CallExpr(ExprPtr expr, vector<Arg> &&a);
  /// One-argument unnamed call constructor (expr(arg1)).
  explicit CallExpr(ExprPtr expr, ExprPtr arg1 = nullptr, ExprPtr arg2 = nullptr,
                    ExprPtr arg3 = nullptr);
  /// Multi-argument unnamed call constructor (expr(exprArgs...)).
  CallExpr(ExprPtr expr, vector<ExprPtr> &&exprArgs);
  CallExpr(const CallExpr &expr);

  string toString() const override;
  ExprPtr clone() const override;
  void accept(ASTVisitor &visitor) const override;

  const CallExpr *getCall() const override { return this; }
};

/// Dot (access) expression (expr.member).
/// @example a.b
struct DotExpr : public Expr {
  ExprPtr expr;
  string member;

  DotExpr(ExprPtr expr, string member);
  DotExpr(const DotExpr &expr);

  string toString() const override;
  ExprPtr clone() const override;
  void accept(ASTVisitor &visitor) const override;

  const DotExpr *getDot() const override { return this; }
};

/// Slice expression (st:stop:step).
/// @example 1:10:3
/// @example s::-1
/// @example :::
struct SliceExpr : public Expr {
  /// Any of these can be nullptr to account for partial slices.
  ExprPtr start, stop, step;

  SliceExpr(ExprPtr start, ExprPtr stop, ExprPtr step);
  SliceExpr(const SliceExpr &expr);

  string toString() const override;
  ExprPtr clone() const override;
  void accept(ASTVisitor &visitor) const override;
};

/// Ellipsis expression.
/// @example ...
struct EllipsisExpr : public Expr {
  /// True if this is a target partial argument within a PipeExpr.
  /// If true, this node will be handled differently during the type-checking stage.
  bool isPipeArg;

  explicit EllipsisExpr(bool isPipeArg = false);
  EllipsisExpr(const EllipsisExpr &expr) = default;

  string toString() const override;
  ExprPtr clone() const override;
  void accept(ASTVisitor &visitor) const override;
};

/// Type-of expression (typeof expr).
/// @example typeof(5)
struct TypeOfExpr : public Expr {
  ExprPtr expr;

  explicit TypeOfExpr(ExprPtr expr);
  TypeOfExpr(const TypeOfExpr &n);

  string toString() const override;
  ExprPtr clone() const override;
  void accept(ASTVisitor &visitor) const override;
};

/// Lambda expression (lambda (vars)...: expr).
/// @example lambda a, b: a + b
struct LambdaExpr : public Expr {
  vector<string> vars;
  ExprPtr expr;

  LambdaExpr(vector<string> &&vars, ExprPtr expr);
  LambdaExpr(const LambdaExpr &);

  string toString() const override;
  ExprPtr clone() const override;
  void accept(ASTVisitor &visitor) const override;
};

/// Yield (send to generator) expression.
/// @example (yield)
struct YieldExpr : public Expr {
  YieldExpr();
  YieldExpr(const YieldExpr &expr) = default;

  string toString() const override;
  ExprPtr clone() const override;
  void accept(ASTVisitor &visitor) const override;
};

/// The following nodes are created after the simplify stage.

/// Statement expression (stmts...; expr).
/// Statements are evaluated only if the expression is evaluated
/// (to support short-circuiting).
/// @example (a = 1; b = 2; a + b)
struct StmtExpr : public Expr {
  vector<unique_ptr<Stmt>> stmts;
  ExprPtr expr;

  StmtExpr(vector<unique_ptr<Stmt>> &&stmts, ExprPtr expr);
  StmtExpr(const StmtExpr &expr);

  string toString() const override;
  ExprPtr clone() const override;
  void accept(ASTVisitor &visitor) const override;
};

/// Pointer expression (__ptr__(expr)).
/// @example __ptr__(a)
struct PtrExpr : public Expr {
  ExprPtr expr;

  explicit PtrExpr(ExprPtr expr);
  PtrExpr(const PtrExpr &expr);

  string toString() const override;
  ExprPtr clone() const override;
  void accept(ASTVisitor &visitor) const override;
};

/// Static tuple indexing expression (expr[index]).
/// @example (1, 2, 3)[2]
struct TupleIndexExpr : Expr {
  ExprPtr expr;
  int index;

  TupleIndexExpr(ExprPtr expr, int index);
  TupleIndexExpr(const TupleIndexExpr &expr);

  string toString() const override;
  ExprPtr clone() const override;
  void accept(ASTVisitor &visitor) const override;
};

/// Static tuple indexing expression (expr[index]).
/// @example (1, 2, 3)[2]
struct InstantiateExpr : Expr {
  ExprPtr typeExpr;
  vector<ExprPtr> typeParams;

  InstantiateExpr(ExprPtr typeExpr, vector<ExprPtr> &&typeParams);
  /// Convenience constructor for a single type parameter.
  InstantiateExpr(ExprPtr typeExpr, ExprPtr typeParam);
  InstantiateExpr(const InstantiateExpr &expr);

  string toString() const override;
  ExprPtr clone() const override;
  void accept(ASTVisitor &visitor) const override;
};

/// Stack allocation expression (__array__[type](expr)).
/// @example __array__[int](5)
struct StackAllocExpr : Expr {
  ExprPtr typeExpr, expr;

  StackAllocExpr(ExprPtr typeExpr, ExprPtr expr);
  StackAllocExpr(const StackAllocExpr &expr);

  string toString() const override;
  ExprPtr clone() const override;
  void accept(ASTVisitor &visitor) const override;
};

/// Static expression (expr). Must evaluate to an integer at the compile-time.
/// @example 5 + 3
/// @example 3 if N > 5 else 2
/// @example len((1, 2))
struct StaticExpr : public Expr {
  ExprPtr expr;
  /// List of static variables within expr (e.g. N if expr is 3 + N).
  set<string> captures;

  StaticExpr(ExprPtr expr, set<string> &&captures);
  StaticExpr(const StaticExpr &expr);

  string toString() const override;
  ExprPtr clone() const override;
  void accept(ASTVisitor &visitor) const override;

  const StaticExpr *getStatic() const override { return this; }
};

} // namespace ast
} // namespace seq
