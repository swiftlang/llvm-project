#include <swift/bridging>

#pragma clang assume_nonnull begin

struct SharedFRT {
  int value;
  int refCount = 1;

  static SharedFRT *create(int value) SWIFT_RETURNS_RETAINED;
  int getValue() const { return value; }

protected:
  SharedFRT(int value) : value(value) {}
} SWIFT_SHARED_REFERENCE(retainSharedFRT, releaseSharedFRT);

void retainSharedFRT(SharedFRT *frt);
void releaseSharedFRT(SharedFRT *frt);

/// The most recent object returned by SharedFRT::create().
extern SharedFRT *_Nullable lastCreatedSharedFRT;

struct ImmortalFRT {
  int value = 2;

  static ImmortalFRT &get();
} SWIFT_IMMORTAL_REFERENCE;

struct UnsafeFRT {
  int value = 3;

  static UnsafeFRT *create();
} SWIFT_UNSAFE_REFERENCE;

struct MemberRefCounted {
  int value;
  int refCount = 1;

  SWIFT_RETURNS_RETAINED MemberRefCounted(int value) : value(value) {}
  void retain() { ++refCount; }
  void release() { --refCount; }
} SWIFT_SHARED_REFERENCE(.retain, .release);

/// Starts with a C++ vtable pointer where a Swift object has its isa.
struct VirtualRefCounted {
  int value;
  int refCount = 1;

  SWIFT_RETURNS_RETAINED VirtualRefCounted(int value) : value(value) {}
  virtual ~VirtualRefCounted() = default;
  virtual void retain() { ++refCount; }
  virtual void release() { --refCount; }
} SWIFT_SHARED_REFERENCE(.retain, .release);

/// Imported as a Swift subclass of VirtualRefCounted.
struct DerivedVirtualRefCounted : VirtualRefCounted {
  int derivedValue;
  int derivedRetains = 0;

  SWIFT_RETURNS_RETAINED DerivedVirtualRefCounted(int value, int derivedValue)
      : VirtualRefCounted(value), derivedValue(derivedValue) {}
  void retain() override {
    ++derivedRetains;
    VirtualRefCounted::retain();
  }
};

struct FirstBase {
  long firstValue = 10;
  virtual ~FirstBase() = default;
};

/// SharedFRT is not at offset 0, so it does not become a Swift superclass and
/// every retain and release has to adjust the pointer.
struct NonPrimaryFRT : FirstBase, SharedFRT {
  long ownValue = 30;

  static NonPrimaryFRT *create(int value) SWIFT_RETURNS_RETAINED;

private:
  NonPrimaryFRT(int value) : SharedFRT(value) {}
};

inline SharedFRT *asSharedFRT(NonPrimaryFRT *frt) SWIFT_RETURNS_UNRETAINED {
  return frt;
}

/// Defined in impl.cpp, so only that compile unit's debug info describes it.
struct SWIFT_SHARED_REFERENCE(retainIncompleteFRT,
                              releaseIncompleteFRT) IncompleteFRT;

IncompleteFRT *createIncompleteFRT(int value) SWIFT_RETURNS_RETAINED;
int getIncompleteFRTValue(IncompleteFRT *frt);
void retainIncompleteFRT(IncompleteFRT *frt);
void releaseIncompleteFRT(IncompleteFRT *frt);

/// Never defined anywhere.
struct SWIFT_SHARED_REFERENCE(retainOpaqueFRT, releaseOpaqueFRT) OpaqueFRT;

OpaqueFRT *createOpaqueFRT() SWIFT_RETURNS_RETAINED;
void retainOpaqueFRT(OpaqueFRT *frt);
void releaseOpaqueFRT(OpaqueFRT *frt);

/// Only used as an Optional in Swift.
struct OptionalOnlyFRT {
  int value;

  static OptionalOnlyFRT *_Nullable create(int value);
} SWIFT_IMMORTAL_REFERENCE;

/// A value type that points to an FRT.
struct HoldsFRT {
  SharedFRT *shared;
  int number;
};

HoldsFRT makeHoldsFRT(SharedFRT *shared);

#pragma clang assume_nonnull end
