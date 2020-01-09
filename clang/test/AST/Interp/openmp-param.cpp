// RUN: %clang_cc1 -verify -fopenmp-simd %s -Wuninitialized
// RUN: %clang_cc1 -verify -fopenmp-simd %s -Wuninitialized -fexperimental-new-constant-interpreter


// expected-note@+1 {{declared here}}
int f(int argc) {
  // expected-error@+2 {{expression is not an integral constant expression}}
  // expected-note@+1 {{read of non-const variable 'argc' is not allowed in a constant expression}}
  #pragma omp simd simdlen (argc)
  for (int i = 0; i < 10; i++);
}
