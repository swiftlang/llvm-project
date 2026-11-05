// RUN: %clang_cc1 -std=c++17 -ast-dump %s | FileCheck --check-prefix=ATTR %s
// RUN: %clang_cc1 -std=c++17 -finterop-lifetime-inference-test-mode -ast-dump %s | FileCheck --check-prefix=STRIP %s

// In test mode, the hand-written lifetime attributes are not attached to the
// AST. They are kept as oracles.

void f(int *p [[clang::noescape]]);
// ATTR: FunctionDecl {{.*}} f
// ATTR-NEXT: ParmVarDecl {{.*}} p
// ATTR-NEXT: NoEscapeAttr
// STRIP: FunctionDecl {{.*}} f
// STRIP-NEXT: ParmVarDecl {{.*}} p
// STRIP-NOT: NoEscapeAttr

int *g(int *p [[clang::lifetimebound]]);
// ATTR: FunctionDecl {{.*}} g
// ATTR-NEXT: ParmVarDecl {{.*}} p
// ATTR-NEXT: LifetimeBoundAttr
// STRIP: FunctionDecl {{.*}} g
// STRIP-NEXT: ParmVarDecl {{.*}} p
// STRIP-NOT: LifetimeBoundAttr

void h(int *p [[clang::lifetime_capture_by(q)]], int *&q);
// ATTR: FunctionDecl {{.*}} h
// ATTR-NEXT: ParmVarDecl {{.*}} p
// ATTR-NEXT: LifetimeCaptureByAttr
// STRIP: FunctionDecl {{.*}} h
// STRIP-NEXT: ParmVarDecl {{.*}} p
// STRIP-NOT: LifetimeCaptureByAttr

struct S {
  int *m() [[clang::lifetimebound]];
  void c(int *&q) [[clang::lifetime_capture_by(q)]];
};
// ATTR: CXXMethodDecl {{.*}} m 'int *() {{\[\[}}clang::lifetimebound{{\]\]}}'
// STRIP: CXXMethodDecl {{.*}} m 'int *()'
// ATTR: CXXMethodDecl {{.*}} c 'void (int *&) {{\[\[}}clang::lifetime_capture_by(q){{\]\]}}'
// STRIP: CXXMethodDecl {{.*}} c 'void (int *&)'

void end();
// STRIP: FunctionDecl {{.*}} end
