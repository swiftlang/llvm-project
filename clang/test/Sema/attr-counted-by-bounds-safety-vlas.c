// RUN: %clang_cc1 -fsyntax-only -fexperimental-bounds-safety -verify %s
// RUN: %clang_cc1 -fsyntax-only -fexperimental-bounds-safety -fexperimental-late-parse-attributes -verify %s
// TO_UPSTREAM(BoundsSafety) ON
// RUN: %clang_cc1 -fsyntax-only -Wno-error=bounds-safety-counted-by-elt-type-unknown-size -fexperimental-bounds-safety -verify=downgrade %s
// RUN: %clang_cc1 -fsyntax-only -Wno-error=bounds-safety-counted-by-elt-type-unknown-size -fexperimental-bounds-safety -fexperimental-late-parse-attributes -verify=downgrade %s
// RUN: %clang_cc1 -fsyntax-only -Wno-bounds-safety-counted-by-elt-type-unknown-size -fexperimental-bounds-safety -verify=suppress %s
// RUN: %clang_cc1 -fsyntax-only -Wno-bounds-safety-counted-by-elt-type-unknown-size -fexperimental-bounds-safety -fexperimental-late-parse-attributes -verify=suppress %s
// TO_UPSTREAM(BoundsSafety) OFF
//
// This is a portion of the `attr-counted-by-vla.c` test but is checked
// under the semantics of `-fexperimental-bounds-safety` which has different
// behavior.

#define __counted_by(f)  __attribute__((counted_by(f)))

struct has_unannotated_VLA {
  int count;
  char buffer[];
};

struct has_annotated_VLA {
  int count;
  char buffer[] __counted_by(count);
};

// TO_UPSTREAM(BoundsSafety) ON

// suppress-no-diagnostics
struct buffer_of_structs_with_unnannotated_vla {
  int count;
  // downgrade-warning@+2{{'counted_by' should not be applied to an array with element of unknown size because 'struct has_unannotated_VLA' is a struct type with a flexible array member. This will be an error in a future compiler version}}
  // expected-error@+1{{'counted_by' should not be applied to an array with element of unknown size because 'struct has_unannotated_VLA' is a struct type with a flexible array member. This will be an error in a future compiler version}}
  struct has_unannotated_VLA Arr[] __counted_by(count);
};


struct buffer_of_structs_with_annotated_vla {
  int count;
  // downgrade-warning@+2{{'counted_by' should not be applied to an array with element of unknown size because 'struct has_annotated_VLA' is a struct type with a flexible array member. This will be an error in a future compiler version}}
  // expected-error@+1{{'counted_by' should not be applied to an array with element of unknown size because 'struct has_annotated_VLA' is a struct type with a flexible array member. This will be an error in a future compiler version}}
  struct has_annotated_VLA Arr[] __counted_by(count);
};

struct buffer_of_const_structs_with_annotated_vla {
  int count;
  // Make sure the `const` qualifier is printed when printing the element type.
  // downgrade-warning@+2{{'counted_by' should not be applied to an array with element of unknown size because 'const struct has_annotated_VLA' is a struct type with a flexible array member. This will be an error in a future compiler version}}
  // expected-error@+1{{'counted_by' should not be applied to an array with element of unknown size because 'const struct has_annotated_VLA' is a struct type with a flexible array member. This will be an error in a future compiler version}}
  const struct has_annotated_VLA Arr[] __counted_by(count);
};

// TO_UPSTREAM(BoundsSafety) OFF
