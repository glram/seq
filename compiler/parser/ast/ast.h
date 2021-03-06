/**
 * ast.h
 * Describes Seq AST.
 */

#pragma once

#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "lang/seq.h"
#include "parser/ast/types.h"
#include "parser/ast/visitor.h"
#include "parser/common.h"

/// Macro that makes node cloneable and visitable
#define NODE_UTILITY(X, Y)                                                     \
  virtual X##Ptr clone() const override { return std::make_unique<Y>(*this); } \
  virtual void accept(ASTVisitor &visitor) const override {                    \
    visitor.visit(this);                                                       \
  }

namespace seq {
namespace ast {

struct Expr;
struct Stmt;
struct Pattern;

typedef std::unique_ptr<Expr> ExprPtr;
typedef std::unique_ptr<Stmt> StmtPtr;
typedef std::unique_ptr<Pattern> PatternPtr;

struct Expr : public seq::SrcObject {
private:
  /// Each expression comes with an associated type.
  /// Types are nullptr until realized by a typechecker.
  types::TypePtr _type;

  /// Flag that indicates is this expression a type expression
  /// (e.g. int, list[int], or generic T)
  bool _isType;

public:
  Expr();
  Expr(const Expr &e);
  virtual ~Expr() {}
  virtual ExprPtr clone() const = 0;

  /// Convert node to a string
  virtual std::string toString() const = 0;
  /// Accept an AST walker/visitor
  virtual void accept(ASTVisitor &) const = 0;

  /// Type utilities
  types::TypePtr getType() const { return _type; }
  void setType(types::TypePtr t) { _type = t; }
  bool isType() const { return _isType; }
  void markType() { _isType = true; }

  /// Allow pretty-printing to C++ streams
  friend std::ostream &operator<<(std::ostream &out, const Expr &c) {
    return out << c.toString();
  }
};

struct Stmt : public seq::SrcObject {
  Stmt() = default;
  Stmt(const Stmt &s);
  Stmt(const seq::SrcInfo &s);
  virtual ~Stmt() {}
  virtual StmtPtr clone() const = 0;

  /// Convert node to a string
  virtual std::string toString() const = 0;
  /// Accept an AST walker/visitor
  virtual void accept(ASTVisitor &) const = 0;

  /// Get child statements (e.g. block contents).
  /// Returns the statement itself if it has no child statements.
  virtual std::vector<Stmt *> getStatements();

  /// Allow pretty-printing to C++ streams
  friend std::ostream &operator<<(std::ostream &out, const Stmt &c) {
    return out << c.toString();
  }
};

struct Pattern : public seq::SrcObject {
  types::TypePtr _type;

  Pattern();
  Pattern(const Pattern &e);
  virtual ~Pattern() {}
  virtual PatternPtr clone() const = 0;

  /// Convert node to a string
  virtual std::string toString() const = 0;
  /// Accept an AST walker/visitor
  virtual void accept(ASTVisitor &) const = 0;

  types::TypePtr getType() const { return _type; }
  void setType(types::TypePtr t) { _type = t; }

  /// Allow pretty-printing to C++ streams
  friend std::ostream &operator<<(std::ostream &out, const Pattern &c) {
    return out << c.toString();
  }
};

/// Type that models the function parameters
/// (name: type = deflt)
struct Param {
  std::string name;
  ExprPtr type;
  ExprPtr deflt;
  Param clone() const;
  std::string toString() const;
};

struct NoneExpr : public Expr {
  NoneExpr();
  NoneExpr(const NoneExpr &e);
  std::string toString() const override;
  NODE_UTILITY(Expr, NoneExpr);
};

struct BoolExpr : public Expr {
  bool value;

  BoolExpr(bool v);
  BoolExpr(const BoolExpr &n);
  std::string toString() const override;
  NODE_UTILITY(Expr, BoolExpr);
};

struct IntExpr : public Expr {
  std::string value;
  /// Number suffix (e.g. "u" for "123u")
  std::string suffix;

  IntExpr(int v);
  IntExpr(const IntExpr &n);
  IntExpr(const std::string &v, const std::string &s = "");
  std::string toString() const override;
  NODE_UTILITY(Expr, IntExpr);
};

struct FloatExpr : public Expr {
  double value;
  /// Number suffix (e.g. "u" for "123u")
  std::string suffix;

  FloatExpr(double v, const std::string &s = "");
  FloatExpr(const FloatExpr &n);
  std::string toString() const override;
  NODE_UTILITY(Expr, FloatExpr);
};

struct StringExpr : public Expr {
  std::string value;

  StringExpr(const std::string &v);
  StringExpr(const StringExpr &n);
  std::string toString() const override;
  NODE_UTILITY(Expr, StringExpr);
};

struct FStringExpr : public Expr {
  std::string value;

  FStringExpr(const std::string &v);
  FStringExpr(const FStringExpr &n);
  std::string toString() const override;
  NODE_UTILITY(Expr, FStringExpr);
};

struct KmerExpr : public Expr {
  std::string value;

  KmerExpr(const std::string &v);
  KmerExpr(const KmerExpr &n);
  std::string toString() const override;
  NODE_UTILITY(Expr, KmerExpr);
};

struct SeqExpr : public Expr {
  /// Sequence prefix (e.g. "p" for "p'AU'")
  std::string prefix;
  std::string value;

  SeqExpr(const std::string &v, const std::string &p = "s");
  SeqExpr(const SeqExpr &n);
  std::string toString() const override;
  NODE_UTILITY(Expr, SeqExpr);
};

struct IdExpr : public Expr {
  std::string value;

  IdExpr(const std::string &v);
  IdExpr(const IdExpr &n);
  std::string toString() const override;
  NODE_UTILITY(Expr, IdExpr);
};

struct UnpackExpr : public Expr {
  /// Unpack expression: (*what)
  ExprPtr what;

  UnpackExpr(ExprPtr w);
  UnpackExpr(const UnpackExpr &n);
  std::string toString() const override;
  NODE_UTILITY(Expr, UnpackExpr);
};

struct TupleExpr : public Expr {
  std::vector<ExprPtr> items;

  TupleExpr(std::vector<ExprPtr> &&i);
  TupleExpr(const TupleExpr &n);
  std::string toString() const override;
  NODE_UTILITY(Expr, TupleExpr);
};

struct ListExpr : public Expr {
  std::vector<ExprPtr> items;

  ListExpr(std::vector<ExprPtr> &&i);
  ListExpr(const ListExpr &n);
  std::string toString() const override;
  NODE_UTILITY(Expr, ListExpr);
};

struct SetExpr : public Expr {
  std::vector<ExprPtr> items;

  SetExpr(std::vector<ExprPtr> &&i);
  SetExpr(const SetExpr &n);
  std::string toString() const override;
  NODE_UTILITY(Expr, SetExpr);
};

struct DictExpr : public Expr {
  struct KeyValue {
    ExprPtr key, value;
    KeyValue clone() const;
  };
  std::vector<KeyValue> items;

  DictExpr(std::vector<KeyValue> &&it);
  DictExpr(const DictExpr &n);
  std::string toString() const override;
  NODE_UTILITY(Expr, DictExpr);
};

struct GeneratorExpr : public Expr {
  /// Generator expression: [expr (loops...)]
  /// where loops are: for vars... in gen (if conds...)?
  enum Kind { Generator, ListGenerator, SetGenerator };
  struct Body {
    std::vector<std::string> vars;
    ExprPtr gen;
    std::vector<ExprPtr> conds;
    Body clone() const;
  };

  Kind kind;
  ExprPtr expr;
  std::vector<Body> loops;

  GeneratorExpr(Kind k, ExprPtr e, std::vector<Body> &&l);
  GeneratorExpr(const GeneratorExpr &n);
  std::string toString() const override;
  NODE_UTILITY(Expr, GeneratorExpr);
};

struct DictGeneratorExpr : public Expr {
  /// Dictionary generator expression: {key: expr (loops...)}
  /// where loops are: for vars... in gen (if conds...)?

  ExprPtr key, expr;
  std::vector<GeneratorExpr::Body> loops;

  DictGeneratorExpr(ExprPtr k, ExprPtr e, std::vector<GeneratorExpr::Body> &&l);
  DictGeneratorExpr(const DictGeneratorExpr &n);
  std::string toString() const override;
  NODE_UTILITY(Expr, DictGeneratorExpr);
};

struct IfExpr : public Expr {
  ExprPtr cond, eif, eelse;

  IfExpr(ExprPtr c, ExprPtr i, ExprPtr e);
  IfExpr(const IfExpr &n);
  std::string toString() const override;
  NODE_UTILITY(Expr, IfExpr);
};

struct UnaryExpr : public Expr {
  std::string op;
  ExprPtr expr;

  UnaryExpr(const std::string &o, ExprPtr e);
  UnaryExpr(const UnaryExpr &n);
  std::string toString() const override;
  NODE_UTILITY(Expr, UnaryExpr);
};

struct BinaryExpr : public Expr {
  std::string op;
  ExprPtr lexpr, rexpr;
  /// Does this expression modify lhs (e.g. a += b)?
  bool inPlace;

  BinaryExpr(ExprPtr l, const std::string &o, ExprPtr r, bool i = false);
  BinaryExpr(const BinaryExpr &n);
  std::string toString() const override;
  NODE_UTILITY(Expr, BinaryExpr);
};

struct PipeExpr : public Expr {
  /// Pipe expression: [op expr]...
  /// The first item has op = ""; others have op = "|>" or op = "||>"

  struct Pipe {
    std::string op;
    ExprPtr expr;
    Pipe clone() const;
  };

  std::vector<Pipe> items;

  PipeExpr(std::vector<Pipe> &&it);
  PipeExpr(const PipeExpr &n);
  std::string toString() const override;
  NODE_UTILITY(Expr, PipeExpr);
};

struct IndexExpr : public Expr {
  ExprPtr expr, index;

  IndexExpr(ExprPtr e, ExprPtr i);
  IndexExpr(const IndexExpr &n);
  std::string toString() const override;
  NODE_UTILITY(Expr, IndexExpr);
};

struct CallExpr : public Expr {
  /// Each argument can have a name (e.g. foo(1, b=5))
  struct Arg {
    std::string name;
    ExprPtr value;
    Arg clone() const;
  };

  ExprPtr expr;
  std::vector<Arg> args;

  CallExpr(ExprPtr e, std::vector<Arg> &&a);
  CallExpr(const CallExpr &n);
  /// Simple call e(a...)
  CallExpr(ExprPtr e, std::vector<ExprPtr> &&a);
  /// Simple call e(arg)
  CallExpr(ExprPtr e, ExprPtr arg = nullptr, ExprPtr arg2 = nullptr,
           ExprPtr arg3 = nullptr);
  std::string toString() const override;
  NODE_UTILITY(Expr, CallExpr);
};

struct DotExpr : public Expr {
  ExprPtr expr;
  std::string member;

  DotExpr(ExprPtr e, const std::string &m);
  DotExpr(const DotExpr &n);
  std::string toString() const override;
  NODE_UTILITY(Expr, DotExpr);
};

struct SliceExpr : public Expr {
  /// Any of these can be nullptr to account for partial slices
  ExprPtr st, ed, step;

  SliceExpr(ExprPtr s, ExprPtr e, ExprPtr st);
  SliceExpr(const SliceExpr &n);
  std::string toString() const override;
  NODE_UTILITY(Expr, SliceExpr);
};

struct EllipsisExpr : public Expr {
  /// Expression ..., currently used in partial calls
  EllipsisExpr();
  EllipsisExpr(const EllipsisExpr &n);

  std::string toString() const override;
  NODE_UTILITY(Expr, EllipsisExpr);
};

struct TypeOfExpr : public Expr {
  ExprPtr expr;

  TypeOfExpr(ExprPtr e);
  TypeOfExpr(const TypeOfExpr &n);
  std::string toString() const override;
  NODE_UTILITY(Expr, TypeOfExpr);
};

struct PtrExpr : public Expr {
  ExprPtr expr;

  PtrExpr(ExprPtr e);
  PtrExpr(const PtrExpr &n);
  std::string toString() const override;
  NODE_UTILITY(Expr, PtrExpr);
};

struct LambdaExpr : public Expr {
  /// Expression: lambda vars...: expr
  std::vector<std::string> vars;
  ExprPtr expr;

  LambdaExpr(std::vector<std::string> v, ExprPtr e);
  LambdaExpr(const LambdaExpr &n);
  std::string toString() const override;
  NODE_UTILITY(Expr, LambdaExpr);
};

struct YieldExpr : public Expr {
  /// Expression: (yield) (send to generator)
  YieldExpr();
  YieldExpr(const YieldExpr &n);

  std::string toString() const override;
  NODE_UTILITY(Expr, YieldExpr);
};

struct SuiteStmt : public Stmt {
  /// Represents list (block) of statements.
  /// Does not necessarily imply new variable block.
  using Stmt::Stmt;

  std::vector<StmtPtr> stmts;

  SuiteStmt(std::vector<StmtPtr> &&s);
  SuiteStmt(StmtPtr s, StmtPtr s2 = nullptr, StmtPtr s3 = nullptr);
  SuiteStmt(const SuiteStmt &s);
  std::string toString() const override;
  std::vector<Stmt *> getStatements() override;
  NODE_UTILITY(Stmt, SuiteStmt);
};

struct PassStmt : public Stmt {
  PassStmt();
  PassStmt(const PassStmt &s);
  std::string toString() const override;
  NODE_UTILITY(Stmt, PassStmt);
};

struct BreakStmt : public Stmt {
  BreakStmt();
  BreakStmt(const BreakStmt &s);
  std::string toString() const override;
  NODE_UTILITY(Stmt, BreakStmt);
};

struct ContinueStmt : public Stmt {
  ContinueStmt();
  ContinueStmt(const ContinueStmt &s);
  std::string toString() const override;
  NODE_UTILITY(Stmt, ContinueStmt);
};

struct ExprStmt : public Stmt {
  ExprPtr expr;

  ExprStmt(ExprPtr e);
  ExprStmt(const ExprStmt &s);
  std::string toString() const override;
  NODE_UTILITY(Stmt, ExprStmt);
};

struct AssignStmt : public Stmt {
  /// Statement: lhs : type = rhs
  ExprPtr lhs, rhs, type;
  /// mustExist indicates that lhs must exist (e.g. a += b).
  bool mustExist;
  /// force controls if lhs will shadow existing lhs or not.
  bool force;

  AssignStmt(ExprPtr l, ExprPtr r, ExprPtr t = nullptr, bool m = false,
             bool f = false);
  AssignStmt(const AssignStmt &s);
  std::string toString() const override;
  NODE_UTILITY(Stmt, AssignStmt);
};

struct DelStmt : public Stmt {
  ExprPtr expr;

  DelStmt(ExprPtr e);
  DelStmt(const DelStmt &s);
  std::string toString() const override;
  NODE_UTILITY(Stmt, DelStmt);
};

struct PrintStmt : public Stmt {
  ExprPtr expr;

  PrintStmt(ExprPtr i);
  PrintStmt(const PrintStmt &s);
  std::string toString() const override;
  NODE_UTILITY(Stmt, PrintStmt);
};

struct ReturnStmt : public Stmt {
  /// Might be nullptr for empty return/yield statements
  ExprPtr expr;

  ReturnStmt(ExprPtr e);
  ReturnStmt(const ReturnStmt &s);
  std::string toString() const override;
  NODE_UTILITY(Stmt, ReturnStmt);
};

struct YieldStmt : public Stmt {
  /// Might be nullptr for empty return/yield statements
  ExprPtr expr;

  YieldStmt(ExprPtr e);
  YieldStmt(const YieldStmt &s);
  std::string toString() const override;
  NODE_UTILITY(Stmt, YieldStmt);
};

struct AssertStmt : public Stmt {
  ExprPtr expr;

  AssertStmt(ExprPtr e);
  AssertStmt(const AssertStmt &s);
  std::string toString() const override;
  NODE_UTILITY(Stmt, AssertStmt);
};

struct WhileStmt : public Stmt {
  /// Statement: while cond: suite
  ExprPtr cond;
  StmtPtr suite;

  WhileStmt(ExprPtr c, StmtPtr s);
  WhileStmt(const WhileStmt &s);
  std::string toString() const override;
  NODE_UTILITY(Stmt, WhileStmt);
};

struct ForStmt : public Stmt {
  /// Statement: for var in iter: suite
  ExprPtr var;
  ExprPtr iter;
  StmtPtr suite;

  ForStmt(ExprPtr v, ExprPtr i, StmtPtr s);
  ForStmt(const ForStmt &s);
  std::string toString() const override;
  NODE_UTILITY(Stmt, ForStmt);
};

struct IfStmt : public Stmt {
  /// Statement: if cond: suite;
  struct If {
    ExprPtr cond;
    StmtPtr suite;
    If clone() const;
  };

  /// Last member of ifs has cond = nullptr (else block)
  std::vector<If> ifs;

  IfStmt(std::vector<If> &&i);
  IfStmt(ExprPtr cond, StmtPtr suite);
  IfStmt(const IfStmt &s);
  std::string toString() const override;
  NODE_UTILITY(Stmt, IfStmt);
};

struct MatchStmt : public Stmt {
  ExprPtr what;
  std::vector<PatternPtr> patterns;
  std::vector<StmtPtr> cases;

  MatchStmt(ExprPtr w, std::vector<PatternPtr> &&p, std::vector<StmtPtr> &&c);
  MatchStmt(ExprPtr w, std::vector<std::pair<PatternPtr, StmtPtr>> &&v);
  MatchStmt(const MatchStmt &s);
  std::string toString() const override;
  NODE_UTILITY(Stmt, MatchStmt);
};

struct ExtendStmt : public Stmt {
  ExprPtr what;
  StmtPtr suite;

  ExtendStmt(ExprPtr e, StmtPtr s);
  ExtendStmt(const ExtendStmt &s);
  std::string toString() const override;
  NODE_UTILITY(Stmt, ExtendStmt);
};

struct ImportStmt : public Stmt {
  typedef std::pair<std::string, std::string> Item;

  /// Statement:
  /// 1. from from[0] import what...
  /// 2. import from[0] as from[1]
  /// where what is: what[0] as what[1]
  Item from;
  std::vector<Item> what;

  ImportStmt(const Item &f, const std::vector<Item> &w);
  ImportStmt(const ImportStmt &s);
  std::string toString() const override;
  NODE_UTILITY(Stmt, ImportStmt);
};

struct ExternImportStmt : public Stmt {
  ImportStmt::Item name;

  ExprPtr from;
  /// Return type for foreign imports
  ExprPtr ret;
  /// Argument types for foreign import
  std::vector<Param> args;
  /// Indicates language (c, py, r)
  std::string lang;

  ExternImportStmt(const ImportStmt::Item &n, ExprPtr f, ExprPtr t,
                   std::vector<Param> &&a, const std::string &l);
  ExternImportStmt(const ExternImportStmt &s);
  std::string toString() const override;
  NODE_UTILITY(Stmt, ExternImportStmt);
};

struct TryStmt : public Stmt {
  struct Catch {
    std::string var;
    ExprPtr exc;
    StmtPtr suite;

    Catch clone() const;
  };

  StmtPtr suite;
  std::vector<Catch> catches;
  StmtPtr finally;

  TryStmt(StmtPtr s, std::vector<Catch> &&c, StmtPtr f);
  TryStmt(const TryStmt &s);
  std::string toString() const override;
  NODE_UTILITY(Stmt, TryStmt);
};

struct GlobalStmt : public Stmt {
  std::string var;

  GlobalStmt(const std::string &v);
  GlobalStmt(const GlobalStmt &s);
  std::string toString() const override;
  NODE_UTILITY(Stmt, GlobalStmt);
};

struct ThrowStmt : public Stmt {
  ExprPtr expr;

  ThrowStmt(ExprPtr e);
  ThrowStmt(const ThrowStmt &s);
  std::string toString() const override;
  NODE_UTILITY(Stmt, ThrowStmt);
};

struct FunctionStmt : public Stmt {
  std::string name;
  ExprPtr ret;
  std::vector<Param> generics;
  std::vector<Param> args;
  std::shared_ptr<Stmt> suite;
  /// List of attributes (e.g. @internal @prefetch)
  std::vector<std::string> attributes;

  FunctionStmt(const std::string &n, ExprPtr r, std::vector<Param> &&g,
               std::vector<Param> &&a, std::shared_ptr<Stmt> s,
               const std::vector<std::string> &at);
  FunctionStmt(const FunctionStmt &s);
  std::string toString() const override;
  NODE_UTILITY(Stmt, FunctionStmt);
};

struct PyDefStmt : public Stmt {
  std::string name;
  ExprPtr ret;
  std::vector<Param> args;
  std::string code;

  PyDefStmt(const std::string &n, ExprPtr r, std::vector<Param> &&a,
            const std::string &s);
  PyDefStmt(const PyDefStmt &s);
  std::string toString() const override;
  NODE_UTILITY(Stmt, PyDefStmt);
};

struct ClassStmt : public Stmt {
  /// Is it type (record) or a class?
  bool isRecord;
  std::string name;
  std::vector<Param> generics;
  std::vector<Param> args;
  StmtPtr suite;
  std::vector<std::string> attributes;

  ClassStmt(bool i, const std::string &n, std::vector<Param> &&g,
            std::vector<Param> &&a, StmtPtr s,
            const std::vector<std::string> &at);
  ClassStmt(const ClassStmt &s);
  std::string toString() const override;
  NODE_UTILITY(Stmt, ClassStmt);
};

struct AssignEqStmt : public Stmt {
  /// Statement: lhs op= rhs
  ExprPtr lhs, rhs;
  std::string op;

  AssignEqStmt(ExprPtr l, ExprPtr r, const std::string &o);
  AssignEqStmt(const AssignEqStmt &s);
  std::string toString() const override;
  NODE_UTILITY(Stmt, AssignEqStmt);
};

struct YieldFromStmt : public Stmt {
  ExprPtr expr;

  YieldFromStmt(ExprPtr e);
  YieldFromStmt(const YieldFromStmt &s);
  std::string toString() const override;
  NODE_UTILITY(Stmt, YieldFromStmt);
};

struct WithStmt : public Stmt {
  std::vector<ExprPtr> items;
  std::vector<std::string> vars;
  StmtPtr suite;

  WithStmt(std::vector<ExprPtr> &&i, const std::vector<std::string> &v,
           StmtPtr s);
  WithStmt(std::vector<std::pair<ExprPtr, std::string>> &&v, StmtPtr s);
  WithStmt(const WithStmt &s);
  std::string toString() const override;
  NODE_UTILITY(Stmt, WithStmt);
};

struct StarPattern : public Pattern {
  StarPattern();
  StarPattern(const StarPattern &p);
  std::string toString() const override;
  NODE_UTILITY(Pattern, StarPattern);
};

struct IntPattern : public Pattern {
  int value;

  IntPattern(int v);
  IntPattern(const IntPattern &p);
  std::string toString() const override;
  NODE_UTILITY(Pattern, IntPattern);
};

struct BoolPattern : public Pattern {
  bool value;

  BoolPattern(bool v);
  BoolPattern(const BoolPattern &p);
  std::string toString() const override;
  NODE_UTILITY(Pattern, BoolPattern);
};

struct StrPattern : public Pattern {
  std::string value;

  StrPattern(const std::string &v);
  StrPattern(const StrPattern &p);
  std::string toString() const override;
  NODE_UTILITY(Pattern, StrPattern);
};

struct SeqPattern : public Pattern {
  std::string value;

  SeqPattern(const std::string &v);
  SeqPattern(const SeqPattern &p);
  std::string toString() const override;
  NODE_UTILITY(Pattern, SeqPattern);
};

struct RangePattern : public Pattern {
  int start, end;

  RangePattern(int s, int e);
  RangePattern(const RangePattern &p);
  std::string toString() const override;
  NODE_UTILITY(Pattern, RangePattern);
};

struct TuplePattern : public Pattern {
  std::vector<PatternPtr> patterns;

  TuplePattern(std::vector<PatternPtr> &&p);
  TuplePattern(const TuplePattern &p);
  std::string toString() const override;
  NODE_UTILITY(Pattern, TuplePattern);
};

struct ListPattern : public Pattern {
  std::vector<PatternPtr> patterns;

  ListPattern(std::vector<PatternPtr> &&p);
  ListPattern(const ListPattern &p);
  std::string toString() const override;
  NODE_UTILITY(Pattern, ListPattern);
};

struct OrPattern : public Pattern {
  std::vector<PatternPtr> patterns;

  OrPattern(std::vector<PatternPtr> &&p);
  OrPattern(const OrPattern &p);
  std::string toString() const override;
  NODE_UTILITY(Pattern, OrPattern);
};

struct WildcardPattern : public Pattern {
  std::string var;

  WildcardPattern(const std::string &v);
  WildcardPattern(const WildcardPattern &p);
  std::string toString() const override;
  NODE_UTILITY(Pattern, WildcardPattern);
};

struct GuardedPattern : public Pattern {
  PatternPtr pattern;
  ExprPtr cond;

  GuardedPattern(PatternPtr p, ExprPtr c);
  GuardedPattern(const GuardedPattern &p);
  std::string toString() const override;
  NODE_UTILITY(Pattern, GuardedPattern);
};

struct BoundPattern : public Pattern {
  std::string var;
  PatternPtr pattern;

  BoundPattern(const std::string &v, PatternPtr p);
  BoundPattern(const BoundPattern &p);
  std::string toString() const override;
  NODE_UTILITY(Pattern, BoundPattern);
};

template <typename T> T CL(const T &v) { return v.clone(); }
template <typename T> std::unique_ptr<T> CL(const std::unique_ptr<T> &v) {
  return v ? v->clone() : nullptr;
}
template <typename T> std::vector<T> CL(const std::vector<T> &v) {
  std::vector<T> r;
  for (auto &i : v)
    r.push_back(CL(i));
  return r;
}
template <typename T>
std::string combine(const std::vector<T> &items, std::string delim = " ") {
  std::string s = "";
  for (int i = 0; i < items.size(); i++)
    s += (i ? delim : "") + items[i]->toString();
  return s;
}

} // namespace ast
} // namespace seq
