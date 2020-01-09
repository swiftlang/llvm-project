// RUN: %clang_cc1 -std=c++2a -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++2a -fsyntax-only %s -verify

union E2 {} emptyUnion; // expected-note {{here}}

constexpr E2 copyUnion(emptyUnion); // expected-error {{constant expression}} \
                                    // expected-note {{read of non-const}} \
                                    // expected-note {{in call}}
