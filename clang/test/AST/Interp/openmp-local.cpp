// RUN: %clang_cc1 -verify -fopenmp-simd %s -Wuninitialized
// RUN: %clang_cc1 -verify -fopenmp-simd %s -Wuninitialized -fexperimental-new-constant-interpreter


int g() {
  int i;
  // expected-note@+1 {{declared here}}
  int &j = i;
  float *f;
  // expected-error@+2 {{expression is not an integral constant expression}}
  // expected-note@+1 {{initializer of 'j' is not a constant expression}}
  #pragma omp simd aligned(f:j)
  for (int k = 0; k < 10; k++);
}
