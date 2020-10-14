#pragma once

#include <deque>
#include <memory>
#include <stack>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "lang/seq.h"
#include "parser/ast/ast.h"
#include "parser/ast/types.h"
#include "parser/common.h"

namespace seq {
namespace ast {

// struct RealizationContext {
//   /// List of class methods and members
//   /// Maps canonical class name to a map of methods and members
//   /// and their generalized types
//   struct ClassBody {
//     // Needs vector as the order is important
//     std::vector<std::pair<std::string, types::TypePtr>> members;
//     std::unordered_map<std::string, std::vector<types::FuncTypePtr>> methods;
//   };
//   std::unordered_map<std::string, ClassBody> classes;

//   struct FuncRealization {
//     std::string fullName;
//     types::FuncTypePtr type;
//     std::shared_ptr<FunctionStmt> ast;
//     seq::BaseFunc *handle;
//     std::string base;
//   };
//   struct ClassRealization {
//     std::string fullName;
//     types::ClassTypePtr type;
//     std::vector<std::pair<std::string, types::ClassTypePtr>> args;
//     seq::types::Type *handle;
//     std::string base;
//   };
//   RealizationContext();

// public: /* Names */
//   /// Name counter (how many times we used a name)
//   /// Used for generating unique name for each identifier
//   /// (e.g. if we have two def foo, one will be known as foo and one as foo.1
//   std::unordered_map<std::string, int> moduleNames;
//   /// Mapping to canonical names
//   /// (each SrcInfo positions maps to a unique canonical name)
//   std::unordered_map<SrcInfo, std::string, SrcInfoHash> canonicalNames;
//   /// Current unbound type ID counter.
//   /// Each unbound variable must have different ID.

// public:
//   /// Generate canonical name for a SrcInfo and original class/function name
//   std::string generateCanonicalName(const std::string &base, const std::string
//   &name); int &getUnboundCount();

// public: /* Lookup */
// public:
//   /// Getters and setters for the method/member/realization lookup tables
//   ClassBody *findClass(const std::string &name);
//   const std::vector<types::FuncTypePtr> *findMethod(const std::string &name,
//                                                     const std::string &method) const;
//   types::TypePtr findMember(const std::string &name, const std::string &member)
//   const;

// public: /** Template ASTs **/
//   /// Template function ASTs.
//   /// Mapping from a canonical function name to a pair of
//   /// generalized function type and the untyped function AST.
//   std::unordered_map<std::string,
//                      std::pair<types::TypePtr, std::shared_ptr<FunctionStmt>>>
//       funcASTs;
//   /// Template class ASTs.
//   /// Mapping from a canonical class name to a pair of
//   /// generalized class type and the untyped class AST.
//   std::unordered_map<std::string, types::TypePtr> classASTs;

// public:
//   std::shared_ptr<Stmt> getAST(const std::string &name) const;

// public: /* Realizations */
//   /// Current function realizations.
//   /// Mapping from a canonical function name to a hashtable
//   /// of realized and fully type-checked function ASTs.
//   std::unordered_map<std::string, std::unordered_map<std::string, FuncRealization>>
//       funcRealizations;
//   /// Current class realizations.
//   /// Mapping from a canonical class name to a hashtable
//   /// of realized and fully type-checked class ASTs.

//   std::unordered_map<std::string, std::unordered_map<std::string, ClassRealization>>
//       classRealizations;

//   // Maps realizedName to canonicalName
//   std::unordered_map<std::string, std::string> realizationLookup;

//   // std::vector<std::set<std::pair<std::string>>>
//   // realizationCache; // add newly realized functions here; useful for jit

// public:
//   std::vector<ClassRealization> getClassRealizations(const std::string &name);
//   std::vector<FuncRealization> getFuncRealizations(const std::string &name);

//   std::unordered_map<std::string, types::TypePtr> globalNames;
//   std::unordered_set<std::string> variardicCache;
// };

template <typename T> class Context : public std::enable_shared_from_this<Context<T>> {
public:
  typedef std::unordered_map<std::string, std::deque<std::shared_ptr<T>>> Map;

protected:
  Map map;
  std::deque<std::vector<std::string>> stack;
  std::unordered_set<std::string> flags;

public:
  typename Map::iterator begin() { return map.begin(); }
  typename Map::iterator end() { return map.end(); }

  std::shared_ptr<T> find(const std::string &name) const {
    auto it = map.find(name);
    if (it != map.end()) {
      return it->second.front();
    }
    return nullptr;
  }
  void add(const std::string &name, std::shared_ptr<T> var) {
    assert(!name.empty());
    map[name].push_front(var);
    stack.front().push_back(name);
  }
  void addToplevel(const std::string &name, std::shared_ptr<T> var) {
    assert(!name.empty());
    map[name].push_back(var);
    stack.back().push_back(name); // add to the latest "level"
  }
  void addLevel() { stack.push_front(std::vector<std::string>()); }
  virtual void addBlock() { addLevel(); }

  void removeFromMap(const std::string &name) {
    auto i = map.find(name);
    assert(!(i == map.end() || !i->second.size()));
    i->second.pop_front();
    if (!i->second.size())
      map.erase(name);
  }

  void removeLevel() {
    for (auto &name : stack.front())
      removeFromMap(name);
    stack.pop_front();
  }
  virtual void popBlock() { removeLevel(); }

  void remove(const std::string &name) {
    removeFromMap(name);
    for (auto &s : stack) {
      auto i = std::find(s.begin(), s.end(), name);
      if (i != s.end()) {
        s.erase(i);
        return;
      }
    }
    assert(false);
  }
  void setFlag(const std::string &s) { flags.insert(s); }
  void unsetFlag(const std::string &s) { flags.erase(s); }
  bool hasFlag(const std::string &s) { return flags.find(s) != flags.end(); }
  bool isToplevel() const { return stack.size() == 1; }

protected:
  std::string filename;

public:
  std::string getFilename() const { return filename; }
  void setFilename(const std::string &f) { filename = f; }

public:
  Context(const std::string &filename) : filename(filename) {}
  virtual ~Context() {}
  virtual void dump(int pad = 0) {}
};

} // namespace ast
} // namespace seq
