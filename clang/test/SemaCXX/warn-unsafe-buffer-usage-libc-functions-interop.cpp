// RUN: %clang_cc1 -std=c++20 -Wno-all -Wunsafe-buffer-usage \
// RUN:            -verify -fexperimental-bounds-safety-attributes %s
#include <ptrcheck.h>
typedef unsigned size_t;
typedef struct {} FILE;

namespace annotated_libc {
  // For libc functions that have annotations,
  // `-Wunsafe-buffer-usage-in-libc-call` yields to the interoperation
  // warnings.

// expected-note@+2{{consider using a safe container and passing '.data()' to the parameter 'dst' and '.size()' to its dependent parameter 'size' or 'std::span' and passing '.first(...).data()' to the parameter 'dst'}}
// expected-note@+1{{consider using a safe container and passing '.data()' to the parameter 'src' and '.size()' to its dependent parameter 'size' or 'std::span' and passing '.first(...).data()' to the parameter 'src'}}
void memcpy(void * __sized_by(size) dst, const void * __sized_by(size) src, unsigned size);
unsigned strlen( const char* __null_terminated str );
// expected-note@+1{{consider using a safe container and passing '.data()' to the parameter 'buffer' and '.size()' to its dependent parameter 'buf_size' or 'std::span' and passing '.first(...).data()' to the parameter 'buffer'}}
int snprintf( char* __counted_by(buf_size) buffer, unsigned buf_size, const char* format, ... );
int snwprintf( char* __counted_by(buf_size) buffer, unsigned buf_size, const char* format, ... );
int vsnprintf( char* __counted_by(buf_size) buffer, unsigned buf_size, const char* format, ... );
int sprintf( char* __counted_by(10) buffer, const char* format, ... );

void test(char * p, char * q, const char * str,
	  const char * __null_terminated safe_str,
	  char * __counted_by(n) safe_p,
	  size_t n,
	  char * __counted_by(10) safe_ten) {
  memcpy(p, q, 10);                  // expected-warning2{{unsafe assignment to function parameter of count-attributed type}}
  snprintf(p, 10, "%s", "hlo");      // expected-warning{{unsafe assignment to function parameter of count-attributed type}}

  // We still warn about unsafe string pointer arguments to printfs:

  snprintf(safe_p, n, "%s", str);  // expected-warning{{function 'snprintf' is unsafe}} expected-note{{string argument is not guaranteed to be null-terminated}}
  snwprintf(safe_p, n, "%s", str); // expected-warning{{function 'snwprintf' is unsafe}} expected-note{{string argument is not guaranteed to be null-terminated}}

  memcpy(safe_p, safe_p, n);               // no warn
  strlen(str);                             // expected-warning{{unsafe assignment to a parameter of '__null_terminated' type; only '__null_terminated' pointers, string literals, and 'std::string::c_str' calls are compatible with '__null_terminated' pointers}}
  snprintf(safe_p, n, "%s", "hlo");        // no warn
  snprintf(safe_p, n, "%s", safe_str);     // no warn
  snwprintf(safe_p, n, "%s", safe_str);    // no warn

  // v-printf functions and sprintf are still warned about because
  // they cannot be fully safe:

  vsnprintf(safe_p, n, "%s", safe_str); // expected-warning{{function 'vsnprintf' is unsafe}} expected-note{{'va_list' is unsafe}}
  sprintf(safe_ten, "%s", safe_str);    // expected-warning{{function 'sprintf' is unsafe}} expected-note{{change to 'snprintf' for explicit bounds checking}}

}
} // namespace annotated_libc

namespace unannotated_libc {
  // The -Wunsafe-buffer-usage analysis considers some printf
  // functions safe, arguments are correctly annotated. Because these
  // functions are harder to be changed to C++ equivalents.
int printf(const char*, ... );
int fprintf (FILE*, const char*, ... );
int snprintf( char*, unsigned, const char*, ... );
int snwprintf( char*, unsigned, const char*, ... );
int vsnprintf( char*, unsigned, const char*, ... );
  // It is convenient to accept functions like `strlen` or `atoi` when
  // they take a __null_termianted argument.
unsigned strlen( const char* );
int atoi( const char* );

void test(const char * __null_terminated safe_str,
         char * __counted_by(n) safe_p,
         size_t n) {
  FILE *file;
  printf("%s", safe_str);
  fprintf(file, "%s", safe_str);
  snprintf(safe_p, n, "%s", safe_str);
  snwprintf(safe_p, n, "%s", safe_str);
  strlen(safe_str);
  atoi(safe_str);
  printf(safe_str);
  strlen(safe_p); // safe_p is not null-terminated  expected-warning{{function 'strlen' is unsafe}}

  // v-printf functions and sprintf are still warned about because
  // they cannot be fully safe:
  vsnprintf(safe_p, n, "%s", safe_str); // expected-warning{{function 'vsnprintf' is unsafe}} expected-note{{'va_list' is unsafe}}
}
} // namespace unannotated_libc
