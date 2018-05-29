#pragma once

#include <c10/guts/Any.h>
#include <vector>

namespace c10 {

// TODO: Don't name this CallStack, that implies that each entry in the
// stack is a frame, but in this case we're allowed to push multiple entries
// on the stack per frame.
/**
 * The stack of a stack-based interpreter.  Arguments (boxed using Any) can be pushed onto
 * the stack, and then popped out at the implementation of a function.  We use this stack
 * to let us write code which is polymorphic (without templates or code generation) over arguments,
 * and can forward said arguments to an underlying function.
 */
class CallStack final {
public:
  template<class T>
  void push(T&& arg) {
    // TODO Why is make_any in detail?
    callStack_.push_back(c10::detail::make_any(std::forward<T>(arg)));
  }

  template<class T>
  T pop() {
    static_assert(!std::is_reference<T>::value, "Cannot pop by reference");
    // TODO Make sure this moving out of the stack works correctly and doesn't copy
    T result = std::move(callStack_.back()).get<T>();
    callStack_.pop_back();
    return result;
  }

private:
  std::vector<Any> callStack_;
};

}
