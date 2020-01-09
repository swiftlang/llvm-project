// RUN: %clang_cc1 -std=c++2a -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++2a -fsyntax-only %s -verify
// expected-no-diagnostics

class LambdaCaptureThis {
public:
  int x = 0;
  int y = 1;
  constexpr bool call() {
    ([this] { x = y; })();
    return x == 1;
  }
};

static_assert(LambdaCaptureThis().call());

/*

constexpr int lamba_capture_val(int x) {
  return ([x] { return x; })();
}
using A = int[10];
using A = int[lamba_capture_val(10)];

constexpr int lambda_capture_ref(int x) {
  int w = 0;
  auto lambda = [x, &w] {
    w = x;
  };
  lambda();
  return w;
}

using B = int[10];
using B = int[lambda_capture_ref(10)];


constexpr int lambda_capture_struct() {
  struct { int x = 10; } s;
  return ([s] { return s.x; })();
}

using C = int[10];
using C = int[lambda_capture_struct()];
*/
