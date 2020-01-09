// RUN: %clang_cc1 -std=c++17 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++17 -fsyntax-only %s -verify
// expected-no-diagnostics

constexpr bool streq(const char *a, const char *b) {
  while (*a && *a == *b)
    ++a, ++b;
  return *a == *b;
}

constexpr bool is_equal() {
  const char *a = "Hello";
  const char *b = "Hello";
  return streq(a, b);
}

static_assert(is_equal());

constexpr bool not_equal() {
  const char *a = "HA";
  const char *b = "HB";
  return streq(a, b);
}

static_assert(!streq("HA", "HB"));
