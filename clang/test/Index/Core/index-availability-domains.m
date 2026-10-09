// RUN: rm -rf %t
// RUN: split-file --leading-lines %s %t

// RUN: c-index-test core -print-source-symbols -- %t/main.m -I %t \
// RUN:   -target arm64-apple-macosx15 -Xclang -ffeature-availability=CmdLineDomain:on \
// RUN:   | FileCheck %t/main.m

// RUN: c-index-test core -print-source-symbols -- %t/main.m -I %t \
// RUN:   -target arm64-apple-macosx15 -Xclang -ffeature-availability=CmdLineDomain:on \
// RUN:   -fmodules -fmodules-cache-path=%t/mcp \
// RUN:   | FileCheck %t/main.m

// RUN: %clang_cc1 -triple arm64-apple-macosx15 -ffeature-availability=CmdLineDomain:on \
// RUN:   -emit-pch -I %t %t/main.m -o %t/main.pch
// RUN: c-index-test core -print-source-symbols -module-file %t/main.pch \
// RUN:   | FileCheck %t/main.m

//--- module.modulemap
module Domains {
  header "domains.h"
  export *
}

//--- domains.h
#include <availability_domain.h>

int dynamic_domain_pred(void);

CLANG_DYNAMIC_AVAILABILITY_DOMAIN(DynamicDomain, dynamic_domain_pred);
CLANG_ENABLED_AVAILABILITY_DOMAIN(EnabledDomain);

//--- main.m
#include "domains.h"

#define AVAILABLE_IN(name) __attribute__((availability(domain:name, 0)))

// CHECK-NOT: | Ref{{.*}}availability_domain_

__attribute__((availability(domain:DynamicDomain, 0))) void availableInDynamicDomain(void);
// CHECK: [[@LINE-1]]:61 | function/C | availableInDynamicDomain |
// CHECK-NEXT: [[@LINE-2]]:36 | variable/C | __clang_availability_domain_DynamicDomain | c:domains.h@__clang_availability_domain_DynamicDomain | {{.*}} | Ref,RelCont | rel: 1
// CHECK-NEXT: RelCont | availableInDynamicDomain |

__attribute__((availability(domain:EnabledDomain, 1))) void unavailableInEnabledDomain(void);
// CHECK: [[@LINE-1]]:61 | function/C | unavailableInEnabledDomain |
// CHECK-NEXT: [[@LINE-2]]:36 | variable/C | __clang_availability_domain_EnabledDomain | c:domains.h@__clang_availability_domain_EnabledDomain | {{.*}} | Ref,RelCont | rel: 1
// CHECK-NEXT: RelCont | unavailableInEnabledDomain |

__attribute__((availability(domain:DynamicDomain, 0)))
__attribute__((availability(domain:EnabledDomain, 0)))
void multipleAttributes(void);
// CHECK: [[@LINE-1]]:6 | function/C | multipleAttributes |
// CHECK-NEXT: [[@LINE-4]]:36 | variable/C | __clang_availability_domain_DynamicDomain | {{.*}} | Ref,RelCont | rel: 1
// CHECK-NEXT: RelCont | multipleAttributes |
// CHECK-NEXT: [[@LINE-5]]:36 | variable/C | __clang_availability_domain_EnabledDomain | {{.*}} | Ref,RelCont | rel: 1
// CHECK-NEXT: RelCont | multipleAttributes |

AVAILABLE_IN(DynamicDomain) void fromMacro(void);
// CHECK: [[@LINE-1]]:34 | function/C | fromMacro |
// CHECK-NEXT: [[@LINE-2]]:14 | variable/C | __clang_availability_domain_DynamicDomain | {{.*}} | Ref,RelCont | rel: 1
// CHECK-NEXT: RelCont | fromMacro |

// A redeclaration inherits the attribute, but does not reference the domain.
void availableInDynamicDomain(void);
// CHECK: [[@LINE-1]]:6 | function/C | availableInDynamicDomain |
// CHECK-NOT: | Ref{{.*}}availability_domain_

// A domain that is defined on the command line has no decl.
__attribute__((availability(domain:CmdLineDomain, 0))) void availableInCmdLineDomain(void);
// CHECK: [[@LINE-1]]:61 | function/C | availableInCmdLineDomain |
// CHECK-NOT: | Ref{{.*}}availability_domain_

__attribute__((availability(domain:DynamicDomain, 0))) int globalVar1, globalVar2;
// CHECK: [[@LINE-1]]:60 | variable/C | globalVar1 |
// CHECK-NEXT: [[@LINE-2]]:36 | variable/C | __clang_availability_domain_DynamicDomain | {{.*}} | Ref,RelCont | rel: 1
// CHECK-NEXT: RelCont | globalVar1 |
// CHECK-NEXT: [[@LINE-4]]:72 | variable/C | globalVar2 |
// CHECK-NEXT: [[@LINE-5]]:36 | variable/C | __clang_availability_domain_DynamicDomain | {{.*}} | Ref,RelCont | rel: 1
// CHECK-NEXT: RelCont | globalVar2 |

enum E {
  e1 __attribute__((availability(domain:EnabledDomain, 0))),
  // CHECK: [[@LINE-1]]:3 | enumerator/C | e1 |
  // CHECK: [[@LINE-2]]:41 | variable/C | __clang_availability_domain_EnabledDomain | {{.*}} | Ref,RelCont | rel: 1
  // CHECK-NEXT: RelCont | e1 |
};

__attribute__((availability(domain:DynamicDomain, 0)))
__attribute__((objc_root_class))
@interface Base
// CHECK: [[@LINE-1]]:12 | class/ObjC | Base |
// CHECK-NEXT: [[@LINE-4]]:36 | variable/C | __clang_availability_domain_DynamicDomain | {{.*}} | Ref,RelCont | rel: 1
// CHECK-NEXT: RelCont | Base |

@property int prop __attribute__((availability(domain:DynamicDomain, 0)));
// CHECK: [[@LINE-1]]:15 | instance-property/ObjC | prop |
// CHECK-NEXT: RelChild | Base |
// CHECK-NEXT: [[@LINE-3]]:55 | variable/C | __clang_availability_domain_DynamicDomain | {{.*}} | Ref,RelCont | rel: 1
// CHECK-NEXT: RelCont | prop |

- (void)method __attribute__((availability(domain:EnabledDomain, 0)));
// CHECK: [[@LINE-1]]:9 | instance-method/ObjC | method |
// CHECK-NEXT: RelChild | Base |
// CHECK-NEXT: [[@LINE-3]]:51 | variable/C | __clang_availability_domain_EnabledDomain | {{.*}} | Ref,RelCont | rel: 1
// CHECK-NEXT: RelCont | method |
@end

// The implementation of a method and the synthesized accessors inherit the
// attributes from the interface.
// CHECK-NOT: | Ref{{.*}}availability_domain_
@implementation Base
- (void)method {}
@end

void queries(void) {
  // CHECK: [[@LINE-1]]:6 | function/C | queries |
  if (@available(domain:DynamicDomain)) {}
  // CHECK-NEXT: [[@LINE-1]]:25 | variable/C | __clang_availability_domain_DynamicDomain | c:domains.h@__clang_availability_domain_DynamicDomain | {{.*}} | Ref,RelCont | rel: 1
  // CHECK-NEXT: RelCont | queries |

  if (__builtin_available(domain:EnabledDomain)) {}
  // CHECK-NEXT: [[@LINE-1]]:34 | variable/C | __clang_availability_domain_EnabledDomain | c:domains.h@__clang_availability_domain_EnabledDomain | {{.*}} | Ref,RelCont | rel: 1
  // CHECK-NEXT: RelCont | queries |

  if (@available(macOS 15, *)) {}
  if (@available(domain:CmdLineDomain)) {}
  // CHECK-NOT: | Ref{{.*}}availability_domain_
}
