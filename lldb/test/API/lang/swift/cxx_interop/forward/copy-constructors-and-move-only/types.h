/// Count the calls to CopyCounted's special members and NonCopyable's move
/// constructor.
extern int numConstructed;
extern int numCopied;
extern int numDestroyed;
extern int numMoved;

struct CopyCounted {
  int value;

  CopyCounted(int value);
  CopyCounted(const CopyCounted &other);
  ~CopyCounted();
};

/// The implicit copy constructor calls CopyCounted's.
struct HasCopyCountedMember {
  CopyCounted member;
  int other;

  HasCopyCountedMember(int value, int other) : member(value), other(other) {}
};

struct VirtualBase {
  int baseValue = 20;
};

struct HasVirtualBase : virtual VirtualBase {
  int ownValue = 21;
};

/// Imported as ~Copyable because the copy constructor is deleted.
struct NonCopyable {
  int x;

  NonCopyable(int x) : x(x) {}
  NonCopyable(const NonCopyable &) = delete;
  NonCopyable(NonCopyable &&other);
  int method(int y) const { return x * y; }
};

struct NonCopyableHolder {
  NonCopyable member;

  NonCopyableHolder(int x) : member(x) {}
  NonCopyableHolder(const NonCopyableHolder &) = delete;
  NonCopyableHolder(NonCopyableHolder &&) = default;
};

struct NonCopyableDerived : NonCopyable {
  NonCopyableDerived(int x) : NonCopyable(x) {}
};

/// Trivially copyable in C++, but annotated as ~Copyable.
struct __attribute__((swift_attr("~Copyable"))) AnnotatedNonCopyable {
  int x;
};
