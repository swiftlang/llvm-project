// RUN: %clang_cc1 -std=c++17 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++17 -fsyntax-only %s -verify

constexpr double d = 1e300;
constexpr char a0 = d;        // expected-error {{must be initialized}} expected-note {{outside the range}}
constexpr int a1 = d;         // expected-error {{must be initialized}} expected-note {{outside the range}}
constexpr short a2 = d;       // expected-error {{must be initialized}} expected-note {{outside the range}}
constexpr long long a3 = d;   // expected-error {{must be initialized}} expected-note {{outside the range}}
constexpr __int128 a4 = d;    // expected-error {{must be initialized}} expected-note {{outside the range}}
