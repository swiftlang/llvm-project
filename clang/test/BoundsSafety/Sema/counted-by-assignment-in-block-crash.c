// RUN: %clang_cc1 -fsyntax-only -fblocks -fbounds-safety -verify %s
// RUN: %clang_cc1 -fsyntax-only -fblocks -fbounds-safety -x objective-c -fexperimental-bounds-safety-objc -verify %s

#include <ptrcheck.h>

// expected-no-diagnostics

typedef unsigned long size_t;

struct accumulator {
  size_t total_size;
  void *__sized_by(total_size) payload;
};

struct counted {
  int count;
  int *__counted_by(count) buf;
};

struct accumulator *new_accumulator(void);
int consume(void *__single p);

typedef void *__single (^create_b)(void);
int find_or_create(create_b create);

// Both members of the group assigned in a block.
int test_sized_by_pair_in_block(size_t n, void *__sized_by(n) buf) {
  return find_or_create(^void *(void) {
    struct accumulator *acc = new_accumulator();
    acc->total_size = n;
    acc->payload = buf;
    return acc;
  });
}

int test_counted_by_pair_in_block(int n, int *__counted_by(n) buf) {
  return find_or_create(^void *(void) {
    struct counted *c = (struct counted *)new_accumulator();
    c->count = n;
    c->buf = buf;
    return c;
  });
}

// The inner block must get its own ParentMap too, not the outer block's.
int test_nested_blocks(size_t n, void *__sized_by(n) buf) {
  return find_or_create(^void *(void) {
    struct accumulator *outer = new_accumulator();
    outer->total_size = n;
    outer->payload = buf;
    (void)find_or_create(^void *(void) {
      struct accumulator *inner = new_accumulator();
      inner->total_size = n;
      inner->payload = buf;
      return inner;
    });
    return outer;
  });
}

struct const_counted {
  int *__counted_by(4) p;
};

struct const_counted *new_const_counted(void);

// Single assignment: constant count, so no paired count assignment.
int test_single_assignment_in_block(int *__counted_by(4) buf) {
  return find_or_create(^void *(void) {
    struct const_counted *s = new_const_counted();
    s->p = buf;
    return s;
  });
}

struct fam {
  int count;
  int elems[__counted_by(count)];
};

// Flexible array member count: flexible-base path.
int test_flexible_array_member_count_in_block(struct fam *__bidi_indexable f,
                                              int n) {
  return find_or_create(^void *(void) {
    f->count = n;
    return 0;
  });
}

// Out-parameter pair, reached through a dereference, not a member access.
int test_out_param_pair_in_block(int *__counted_by(*out_cnt) *out_buf,
                                 int *out_cnt, int n,
                                 int *__counted_by(n) buf) {
  return find_or_create(^void *(void) {
    *out_buf = buf;
    *out_cnt = n;
    return 0;
  });
}

struct range {
  void *end;
  void *__ended_by(end) start;
};

struct range *new_range(void);

// __ended_by builds a RangeDepGroup, a different group class, same finalization.
int test_ended_by_pair_in_block(void *s, void *e) {
  return find_or_create(^void *(void) {
    struct range *r = new_range();
    r->end = e;
    r->start = s;
    return r;
  });
}

// Count and pointer assigned through captured __block storage.
int test_block_captured_locals(int n, int *__counted_by(n) buf) {
  __block int cnt = 0;
  __block int *__counted_by(cnt) p = 0;
  return find_or_create(^void *(void) {
    cnt = n;
    p = buf;
    return p;
  });
}

// A plain return inside a block, with no bounds attributes anywhere. Once a
// block body is its own analysis unit, AC.getDecl() is a BlockDecl, so this
// reaches TraverseReturnStmt.
int test_plain_return_in_block(void) {
  return find_or_create(^void *(void) {
    return 0;
  });
}

// A local *initialized* in a block.
int test_local_init_in_block(int *__counted_by(4) buf) {
  return find_or_create(^void *(void) {
    int *__counted_by(4) p = buf;
    return p;
  });
}

// A block nested in a statement expression.
int test_block_in_stmt_expr(size_t n, void *__sized_by(n) buf) {
  return ({
    find_or_create(^void *(void) {
      struct accumulator *acc = new_accumulator();
      acc->total_size = n;
      acc->payload = buf;
      return acc;
    });
  });
}
