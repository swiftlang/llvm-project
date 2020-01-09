// RUN: %clang_cc1 -std=c++17 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++17 -fsyntax-only %s -verify
// expected-no-diagnostics

enum EnumChar : char { ENUM_CHAR = 0 };

enum EnumShort : signed short { ENUM_SHORT = 256 };

enum EnumInt : int { ENUM_INT = 65536 };

enum EnumLong : long { ENUM_LONG = 4294967296};


constexpr EnumChar enum_char() {
  return ENUM_CHAR;
}

constexpr EnumShort enum_short() {
  return ENUM_SHORT;
}

constexpr EnumInt enum_int() {
  return ENUM_INT;
}

constexpr EnumLong enum_long() {
  return ENUM_LONG;
}
