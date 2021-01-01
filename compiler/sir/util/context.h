#pragma once

#include <vector>

namespace seq {
namespace ir {
namespace util {

/// Base for SIR visitor contexts.
template <typename Frame> class SIRContext {
private:
  std::vector<Frame> frames;

public:
  template <typename... Args> void emplaceFrame(Args&&... args) {
    frames.emplace_back(std::forward<Args>(args)...);
  }
  void pushFrame(Frame f) { frames.push_back(std::move(f)); }
  void replaceFrame(Frame newFrame) {
    frames.pop_back();
    frames.push_back(newFrame);
  }
  const std::vector<Frame> &getFrames() const { return frames; }
  std::vector<Frame> &getFrames() { return frames; }

  const Frame &getFrame() const { return frames.back(); }
  Frame &getFrame() { return frames.back(); }

  void popFrame() { return frames.pop_back(); }
};

} // namespace util
} // namespace ir
} // namespace seq
