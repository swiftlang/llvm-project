// RUN: %clang_cc1 -std=c++20 -Wno-all -Wunsafe-buffer-usage \
// RUN:            -verify %s
// RUN: %clang_cc1 -std=c++20 -Wno-all -Wunsafe-buffer-usage \
// RUN:            -verify %s -x objective-c++
// RUN: %clang_cc1 -std=c++20 -Wno-all -Wunsafe-buffer-usage-in-libc-call \
// RUN:            -verify %s
// RUN: %clang_cc1 -std=c++20 -Wno-all -Wunsafe-buffer-usage \
// RUN:            -verify -fexperimental-bounds-safety-attributes %s -DINTEROP_MODE

#ifndef INTEROP_MODE
typedef struct {} FILE;
void memcpy();
void __asan_memcpy();
void strcpy();
void strcpy_s();
void wcscpy_s();
unsigned strlen( const char* str );
int fprintf( FILE* stream, const char* format, ... );
int printf( const char* format, ... );
int sprintf( char* buffer, const char* format, ... );
int swprintf( char* buffer, const char* format, ... );
int snprintf( char* buffer, unsigned buf_size, const char* format, ... );
int snwprintf( char* buffer, unsigned buf_size, const char* format, ... );
int snwprintf_s( char* buffer, unsigned buf_size, const char* format, ... );
int vsnprintf( char* buffer, unsigned buf_size, const char* format, ... );
int sscanf_s(const char * buffer, const char * format, ...);
int sscanf(const char * buffer, const char * format, ... );
int wprintf(const wchar_t* format, ... );
int __asan_printf();

namespace std {
  template< class InputIt, class OutputIt >
  OutputIt copy( InputIt first, InputIt last,
		 OutputIt d_first );

  struct iterator{};
  template<typename T>
  struct span {
    T * ptr;
    T * data();
    unsigned size_bytes();
    unsigned size();
    iterator begin() const noexcept;
    iterator end() const noexcept;
  };

  template<typename T>
  struct basic_string {
    T* p;
    T *c_str();
    T *data();
    unsigned size_bytes();
  };

  typedef basic_string<char> string;
  typedef basic_string<wchar_t> wstring;

  // C function under std:
  void memcpy();
  void strcpy();
}

void f(char * p, char * q, std::span<char> s, std::span<char> s2) {
  typedef FILE * _Nullable aligned_file_ptr_t __attribute__((align_value(64)));
  typedef char * _Nullable aligned_char_ptr_t __attribute__((align_value(64)));
  aligned_file_ptr_t fp;
  aligned_char_ptr_t cp;

  memcpy();                   // expected-warning{{function 'memcpy' is unsafe}}
  std::memcpy();              // expected-warning{{function 'memcpy' is unsafe}}
  __builtin_memcpy(p, q, 64); // expected-warning{{function '__builtin_memcpy' is unsafe}}
  __builtin___memcpy_chk(p, q, 8, 64);  // expected-warning{{function '__builtin___memcpy_chk' is unsafe}}
  __asan_memcpy();                      // expected-warning{{function '__asan_memcpy' is unsafe}}
  strcpy();                   // expected-warning{{function 'strcpy' is unsafe}}
  std::strcpy();              // expected-warning{{function 'strcpy' is unsafe}}
  strcpy_s();                 // expected-warning{{function 'strcpy_s' is unsafe}}
  wcscpy_s();                 // expected-warning{{function 'wcscpy_s' is unsafe}}

  /* Test printfs */
  fprintf((FILE*)p, "%s%d", p, *p);  // expected-warning{{function 'fprintf' is unsafe}} expected-note{{string argument is not guaranteed to be null-terminated}}
  printf("%s%d", // expected-warning{{function 'printf' is unsafe}}
	 p,    // expected-note{{string argument is not guaranteed to be null-terminated}} note attached to the unsafe argument
	 *p);
  printf(cp, p, *p); // expected-warning{{function 'printf' is unsafe}} // expected-note{{string argument is not guaranteed to be null-terminated}}
  sprintf(q, "%s%d", "hello", *p); // expected-warning{{function 'sprintf' is unsafe}} expected-note{{change to 'snprintf' for explicit bounds checking}}
  swprintf(q, "%s%d", "hello", *p); // expected-warning{{function 'swprintf' is unsafe}} expected-note{{change to 'snprintf' for explicit bounds checking}}
  snprintf(q, 10, "%s%d", "hello", *p); // expected-warning{{function 'snprintf' is unsafe}} expected-note{{buffer pointer and size may not match}}
  snprintf(cp, 10, "%s%d", "hello", *p); // expected-warning{{function 'snprintf' is unsafe}} expected-note{{buffer pointer and size may not match}}
  snprintf(s.data(), s2.size(), "%s%d", "hello", *p); // expected-warning{{function 'snprintf' is unsafe}} expected-note{{buffer pointer and size may not match}}
  snprintf(nullptr, 0, "");                // expected-warning{{function 'snprintf' is unsafe}} expected-note{{buffer pointer and size may not match}}
  printf(nullptr);                         // expected-warning{{function 'printf' is unsafe}} expected-note{{string argument is not guaranteed to be null-terminated}}
  printf("%s", nullptr);                   // expected-warning{{function 'printf' is unsafe}} expected-note{{string argument is not guaranteed to be null-terminated}}
  snwprintf(s.data(), s2.size(), "%s%d", "hello", *p); // expected-warning{{function 'snwprintf' is unsafe}} expected-note{{buffer pointer and size may not match}}
  snwprintf_s(                      // expected-warning{{function 'snwprintf_s' is unsafe}}
	      s.data(),             // expected-note{{buffer pointer and size may not match}} // note attached to the buffer
	      s2.size(),
	      "%s%d", "hello", *p);
  vsnprintf(s.data(), s.size_bytes(), "%s%d", "hello", *p); // expected-warning{{function 'vsnprintf' is unsafe}} expected-note{{'va_list' is unsafe}}
  sscanf(p, "%s%d", "hello", *p);    // expected-warning{{function 'sscanf' is unsafe}}
  sscanf_s(p, "%s%d", "hello", *p);  // expected-warning{{function 'sscanf_s' is unsafe}}
  fprintf((FILE*)p, "%P%d%p%i hello world %32s", *p, *p, p, *p, p); // expected-warning{{function 'fprintf' is unsafe}} expected-note{{string argument is not guaranteed to be null-terminated}}
  fprintf(fp, "%P%d%p%i hello world %32s", *p, *p, p, *p, p); // expected-warning{{function 'fprintf' is unsafe}} expected-note{{string argument is not guaranteed to be null-terminated}}
  wprintf(L"hello %s", p); // expected-warning{{function 'wprintf' is unsafe}} expected-note{{string argument is not guaranteed to be null-terminated}}


  char a[10], b[11];
  int c[10];
  std::wstring WS;

  snprintf(a, sizeof(b), "%s", __PRETTY_FUNCTION__);         // expected-warning{{function 'snprintf' is unsafe}} expected-note{{buffer pointer and size may not match}}
  snprintf((char*)c, sizeof(c), "%s", __PRETTY_FUNCTION__);  // expected-warning{{function 'snprintf' is unsafe}} expected-note{{buffer pointer and size may not match}}
  fprintf((FILE*)p, "%P%d%p%i hello world %32s", *p, *p, p, *p, "hello"); // no warn
  fprintf(fp, "%P%d%p%i hello world %32s", *p, *p, p, *p, "hello"); // no warn
  printf("%s%d", "hello", *p); // no warn
  snprintf(s.data(), s.size_bytes(), "%s%d", "hello", *p); // no warn
  snprintf(s.data(), s.size_bytes(), "%s%d", __PRETTY_FUNCTION__, *p); // no warn
  snwprintf(s.data(), s.size_bytes(), "%s%d", __PRETTY_FUNCTION__, *p); // no warn
  snwprintf_s(s.data(), s.size_bytes(), "%s%d", __PRETTY_FUNCTION__, *p); // no warn
  wprintf(L"hello %ls", L"world"); // no warn
  wprintf(L"hello %ls", WS.c_str()); // no warn
  strlen("hello");// no warn
  __asan_printf();// a printf but no argument, so no warn
}

void safe_examples(std::string s1, int *p) {
  snprintf(s1.data(), s1.size_bytes(), "%s%d%s%p%s", __PRETTY_FUNCTION__, *p, "hello", p, s1.c_str()); // no warn
  snprintf(s1.data(), s1.size_bytes(), s1.c_str(), __PRETTY_FUNCTION__, *p, "hello", s1.c_str());      // no warn
  printf("%s%d%s%p%s", __PRETTY_FUNCTION__, *p, "hello", p, s1.c_str());              // no warn
  printf(s1.c_str(), __PRETTY_FUNCTION__, *p, "hello", s1.c_str());                   // no warn
  fprintf((FILE*)0, "%s%d%s%p%s", __PRETTY_FUNCTION__, *p, "hello", p, s1.c_str());   // no warn
  fprintf((FILE*)0, s1.c_str(), __PRETTY_FUNCTION__, *p, "hello", s1.c_str());        // no warn

  char a[10];

  snprintf(a, sizeof a, "%s%d%s%p%s", __PRETTY_FUNCTION__, *p, "hello", s1.c_str());         // no warn
  snprintf(a, sizeof(decltype(a)), "%s%d%s%p%s", __PRETTY_FUNCTION__, *p, "hello", s1.c_str());          // no warn
  snprintf(a, 10, "%s%d%s%p%s", __PRETTY_FUNCTION__, *p, "hello", s1.c_str());                // no warn
}


void g(char *begin, char *end, char *p, std::span<char> s) {
  std::copy(begin, end, p); // no warn
  std::copy(s.begin(), s.end(), s.begin()); // no warn
}

// warning gets turned off
void ff(char * p, char * q, std::span<char> s, std::span<char> s2) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-libc-call"
  memcpy();
  std::memcpy();
  __builtin_memcpy(p, q, 64);
  __builtin___memcpy_chk(p, q, 8, 64);
  __asan_memcpy();
  strcpy();
  std::strcpy();
  strcpy_s();
  wcscpy_s();
#pragma clang diagnostic pop
}

#else

#include <ptrcheck.h>
typedef unsigned size_t;

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

#endif //#ifdef INTEROP_MODE
