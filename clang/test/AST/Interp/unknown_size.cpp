// RUN: %clang_cc1 -std=c++17 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++17 -fsyntax-only %s -verify


extern int arr[];
constexpr int *a = arr;
constexpr int *c = &arr[1]; // expected-error {{constant}} expected-note {{indexing of array without known bound}}
constexpr int *d = &a[1]; // expected-error {{constant}} expected-note {{indexing of array without known bound}}
constexpr int *e = a + 1; // expected-error {{constant}} expected-note {{indexing of array without known bound}}

static unsigned __hash_len_0_to_16(const char *__s) {
  const unsigned char __a = __s[0];
  return 0;
}
