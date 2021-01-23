#pragma once

#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

#include "util/common.h"

#include "util/fmt/format.h"
#include "util/fmt/ostream.h"

namespace seq {
namespace ir {

/// Base for SIR attributes.
struct Attribute {
  virtual ~Attribute() noexcept = default;

  /// @return true if the attribute should be propagated across clones
  virtual bool needsClone() const { return true; }

  friend std::ostream &operator<<(std::ostream &os, const Attribute &a) {
    return a.doFormat(os);
  }

  /// @return a clone of the attribute
  std::unique_ptr<Attribute> clone() const {
    return std::unique_ptr<Attribute>(doClone());
  }

private:
  virtual Attribute *doClone() const = 0;

  virtual std::ostream &doFormat(std::ostream &os) const = 0;
};

using AttributePtr = std::unique_ptr<Attribute>;

/// Attribute containing SrcInfo
struct SrcInfoAttribute : public Attribute {
  static const std::string AttributeName;

  /// source info
  seq::SrcInfo info;

  /// Constructs a SrcInfoAttribute.
  /// @param info the source info
  explicit SrcInfoAttribute(seq::SrcInfo info) : info(std::move(info)) {}
  SrcInfoAttribute() = default;

private:
  Attribute *doClone() const override { return new SrcInfoAttribute(*this); }

  std::ostream &doFormat(std::ostream &os) const override { return os << info; }
};

/// Attribute containing function information
struct FuncAttribute : public Attribute {
  static const std::string AttributeName;

  /// attributes map
  std::map<std::string, std::string> attributes;

  /// Constructs a FuncAttribute.
  /// @param attributes the map of attributes
  explicit FuncAttribute(std::map<std::string, std::string> attributes)
      : attributes(std::move(attributes)) {}
  FuncAttribute() = default;

  /// @return true if the map contains val, false otherwise
  bool has(const std::string &val) const;

private:
  Attribute *doClone() const override { return new FuncAttribute(*this); }

  std::ostream &doFormat(std::ostream &os) const override;
};

/// Attribute containing type member information
struct MemberAttribute : public Attribute {
  static const std::string AttributeName;

  /// member source info map
  std::map<std::string, SrcInfo> memberSrcInfo;

  /// Constructs a FuncAttribute.
  /// @param attributes the map of attributes
  explicit MemberAttribute(std::map<std::string, SrcInfo> memberSrcInfo)
      : memberSrcInfo(std::move(memberSrcInfo)) {}

  MemberAttribute() = default;

private:
  Attribute *doClone() const override { return new MemberAttribute(*this); }

  std::ostream &doFormat(std::ostream &os) const override;
};

} // namespace ir
} // namespace seq
