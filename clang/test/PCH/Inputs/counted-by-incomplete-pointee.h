#define __counted_by(f) __attribute__((counted_by(f)))

struct IncompleteTy;

struct CBBuf {
  int count;
  struct IncompleteTy *__counted_by(count) buf;
};
