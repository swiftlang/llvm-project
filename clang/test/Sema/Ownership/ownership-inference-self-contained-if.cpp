// RUN: %clang_cc1 -fsyntax-only \
// RUN:     -flifetime-safety-inference -fexperimental-lifetime-safety-tu-analysis \
// RUN:     -Wlifetime-safety-experimental-field-origin-escape \
// RUN:     %s -verify -Wownership-inference

// SELF_CONTAINED_IF(T, ...) declares that a container is self-contained exactly
// when its named template arguments are. It decomposes the specialization into
// those arguments instead of walking the container's own bases and fields, so
// opaque storage the field walk cannot see through can still be classified. The
// verdict warning anchors at the template definition; the decomposition note
// anchors at the named template parameter.
#define SELF_CONTAINED_IF(...) \
  __attribute__((swift_attr("escapable_if:" #__VA_ARGS__)))

// A scalar element is self-contained. The void* storage and out-of-line
// destructor would otherwise make this 'unknown'; the claim bypasses them, so
// no 'holds raw-pointer field' / 'opaque destructor' notes appear.
// expected-warning@+2 {{type 'VectorScalar<int>' was inferred as 'self-contained'}}
template <class T>
struct SELF_CONTAINED_IF(T) VectorScalar {
  void *storage;
  ~VectorScalar();
};
VectorScalar<int> v_scalar;

// A raw-pointer element: a container of borrowed pointers is a view.
// expected-warning@+3 {{type 'VectorPtr<int *>' was inferred as 'view'}}
// expected-note@+1 {{decomposed into template argument 'T' of type 'int *' that was inferred as 'view'}}
template <class T>
struct SELF_CONTAINED_IF(T) VectorPtr {
  void *storage;
  ~VectorPtr();
};
VectorPtr<int *> v_ptr;

// A record element that is itself a view (its own verdict is reported here).
// expected-warning@+1 {{type 'SomeView' was inferred as 'view'}}
struct SomeView {
  int *p; // expected-note {{holds unmanaged raw-pointer field 'p'}}
};

// The verdict decomposes to the view element; the note names the argument.
// expected-warning@+3 {{type 'VectorView<SomeView>' was inferred as 'view'}}
// expected-note@+1 {{decomposed into template argument 'T' of type 'SomeView' that was inferred as 'view'}}
template <class T>
struct SELF_CONTAINED_IF(T) VectorView {
  void *storage;
  ~VectorView();
};
VectorView<SomeView> v_view;

// Multiple parameters: the verdict is the join, so the most-constraining
// argument decides and only it is blamed (Key=int is trivial, Val=int* a view).
// expected-warning@+3 {{type 'Map<int, int *>' was inferred as 'view'}}
// expected-note@+1 {{decomposed into template argument 'Val' of type 'int *' that was inferred as 'view'}}
template <class Key, class Val>
struct SELF_CONTAINED_IF(Key, Val) Map {
  void *storage;
  ~Map();
};
Map<int, int *> m;

// The claim is honored on a partial specialization: its instantiation decomposes
// through the named parameter just like a full specialization.
template <class T, int N>
struct Buffer;

// expected-warning@+2 {{type 'Buffer<int, 0>' was inferred as 'self-contained'}}
template <class T>
struct SELF_CONTAINED_IF(T) Buffer<T, 0> {
  void *storage;
  ~Buffer();
};
Buffer<int, 0> b;

// Hard error: the claim may only be declared on a class template. The claim does
// not resolve, so inference falls back to the ordinary walk (empty => trivial).
// expected-error@+2 {{conditional self-containedness may only be declared on a class template}}
// expected-warning@+1 {{type 'NotATemplate' was trivially inferred as 'self-contained'}}
struct SELF_CONTAINED_IF(T) NotATemplate {};

// Hard error: a named parameter the template does not have. Reported once at the
// template definition; no instantiation is needed to surface it.
template <class T>
struct SELF_CONTAINED_IF(Nope) BadName { // expected-error {{'Nope' does not name a type parameter of this template}}
  T val;
};

// An explicit instantiation -- and the `extern template` form WebKit uses for
// e.g. mpark::variant -- only inherits the claim from the primary template; it
// does not re-declare it, so it must NOT re-trigger the "only on a class
// template" error. Inference still classifies the instantiation normally.
template <class T>
struct SELF_CONTAINED_IF(T) ExplicitBox {
  void *storage;
  ~ExplicitBox();
};
// expected-warning@+1 {{type 'ExplicitBox<int>' was inferred as 'self-contained'}}
extern template struct ExplicitBox<int>;
// expected-warning@+1 {{type 'ExplicitBox<char>' was inferred as 'self-contained'}}
template struct ExplicitBox<char>;
