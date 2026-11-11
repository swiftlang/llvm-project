// For now, take out this warning from -Weverything. See rdar://190623069

// DEFINE: %{common} = -fsyntax-only -std=c++20 -Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-unused -Wno-missing-prototypes -Wno-dangling
//
// RUN: %clang_cc1 %{common} -Weverything -verify=none %s
// RUN: %clang_cc1 %{common} -Weverything -Wlifetime-safety -verify=none %s
// RUN: %clang_cc1 %{common} -Weverything -Wlifetime-safety \
// RUN:      -Wlifetime-safety-experimental-destructor-use -verify=enabled %s
// RUN: %clang_cc1 %{common} -Weverything -Wlifetime-safety \
// RUN:      -Wlifetime-safety-experimental -verify=enabled %s
// RUN: %clang_cc1 %{common} -Weverything -Wlifetime-safety \
// RUN:      -Wlifetime-safety-experimental-destructor-use \
// RUN:      -Wno-lifetime-safety-experimental-destructor-use -verify=none %s
// RUN: %clang_cc1 %{common} -Wlifetime-safety \
// RUN:      -Wlifetime-safety-experimental-destructor-use -verify=enabled %s
// none-no-diagnostics

struct [[gsl::Owner]] Str {
  ~Str();
};

struct [[gsl::Pointer]] View {
  View(const Str &s [[clang::lifetimebound]]);
};

struct [[gsl::Pointer]] Ref {
  View v;
  Ref();
  Ref &operator=(View v [[clang::lifetime_capture_by_this]]);
  ~Ref();
};

void dtor_reverse_order() {
  Ref r;
  Str backing;
  r = backing; // enabled-warning {{local variable 'backing' does not live long enough}}
} // enabled-note {{local variable 'backing' is destroyed here}} enabled-note {{later used by the destructor of 'r' at scope exit}}
