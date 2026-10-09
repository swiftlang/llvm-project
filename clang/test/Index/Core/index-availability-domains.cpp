// RUN: c-index-test core -print-source-symbols -- %s -std=c++17 \
// RUN:   -target arm64-apple-macosx15 | FileCheck %s

#include <availability_domain.h>

CLANG_ENABLED_AVAILABILITY_DOMAIN(EnabledDomain);

// CHECK-NOT: | Ref{{.*}}availability_domain_

template <typename T>
__attribute__((availability(domain:EnabledDomain, 0))) void functionTemplate(T t);
// CHECK: [[@LINE-1]]:61 | function/C | functionTemplate |
// CHECK-NEXT: [[@LINE-2]]:36 | variable/C | __clang_availability_domain_EnabledDomain | c:index-availability-domains.cpp@__clang_availability_domain_EnabledDomain | {{.*}} | Ref,RelCont | rel: 1
// CHECK-NEXT: RelCont | functionTemplate |

template <typename T>
struct __attribute__((availability(domain:EnabledDomain, 0))) ClassTemplate {};
// CHECK: [[@LINE-1]]:63 | struct(Gen)/C++ | ClassTemplate |
// CHECK-NEXT: [[@LINE-2]]:43 | variable/C | __clang_availability_domain_EnabledDomain | {{.*}} | Ref,RelCont | rel: 1
// CHECK-NEXT: RelCont | ClassTemplate |

struct S {
  __attribute__((availability(domain:EnabledDomain, 0))) void method();
  // CHECK: [[@LINE-1]]:63 | instance-method/C++ | method |
  // CHECK-NEXT: RelChild | S |
  // CHECK-NEXT: [[@LINE-3]]:38 | variable/C | __clang_availability_domain_EnabledDomain | {{.*}} | Ref,RelCont | rel: 1
  // CHECK-NEXT: RelCont | method |

  __attribute__((availability(domain:EnabledDomain, 0))) static int staticVar;
  // CHECK: [[@LINE-1]]:69 | static-property/C++ | staticVar |
  // CHECK-NEXT: RelChild | S |
  // CHECK-NEXT: [[@LINE-3]]:38 | variable/C | __clang_availability_domain_EnabledDomain | {{.*}} | Ref,RelCont | rel: 1
  // CHECK-NEXT: RelCont | staticVar |
};

// A definition outside the class inherits the attribute.
void S::method() {}
// CHECK: [[@LINE-1]]:9 | instance-method/C++ | method |
// CHECK-NOT: | Ref{{.*}}availability_domain_

template <typename T>
void dependentBody(T t) {
  // CHECK: [[@LINE-1]]:6 | function/C | dependentBody |
  if (__builtin_available(domain:EnabledDomain)) {}
  // CHECK: [[@LINE-1]]:34 | variable/C | __clang_availability_domain_EnabledDomain | {{.*}} | Ref,RelCont | rel: 1
  // CHECK-NEXT: RelCont | dependentBody |
}
// CHECK-NOT: | Ref{{.*}}availability_domain_

void instantiate() { dependentBody(1); }
