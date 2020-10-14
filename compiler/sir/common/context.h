#pragma once

#include <vector>

namespace seq {
namespace ir {
namespace common {

template <typename Frame> class IRContext {
private:
  std::vector<Frame> frames;

public:
  template <typename... Args> void pushFrame(Args... args) {
    frames.emplace_back(args...);
  }
  Frame &getFrame() { return frames.back(); }
  void popFrame() { return frames.pop_back(); }
};

} // namespace common
} // namespace ir
} // namespace seq