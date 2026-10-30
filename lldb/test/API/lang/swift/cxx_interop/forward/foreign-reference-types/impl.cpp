#include "frt.h"

SharedFRT *lastCreatedSharedFRT = nullptr;

SharedFRT *SharedFRT::create(int value) {
  lastCreatedSharedFRT = new SharedFRT(value);
  return lastCreatedSharedFRT;
}

void retainSharedFRT(SharedFRT *frt) { ++frt->refCount; }
void releaseSharedFRT(SharedFRT *frt) { --frt->refCount; }

ImmortalFRT &ImmortalFRT::get() {
  static ImmortalFRT instance;
  return instance;
}

UnsafeFRT *UnsafeFRT::create() { return new UnsafeFRT(); }

NonPrimaryFRT *NonPrimaryFRT::create(int value) {
  return new NonPrimaryFRT(value);
}

struct IncompleteFRT {
  int value;
  int refCount;
};

IncompleteFRT *createIncompleteFRT(int value) {
  return new IncompleteFRT{value, 1};
}

int getIncompleteFRTValue(IncompleteFRT *frt) { return frt->value; }
void retainIncompleteFRT(IncompleteFRT *frt) { ++frt->refCount; }
void releaseIncompleteFRT(IncompleteFRT *frt) { --frt->refCount; }

static int opaqueStorage = 14;
static int opaqueRefCount = 1;

OpaqueFRT *createOpaqueFRT() {
  return reinterpret_cast<OpaqueFRT *>(&opaqueStorage);
}
void retainOpaqueFRT(OpaqueFRT *) { ++opaqueRefCount; }
void releaseOpaqueFRT(OpaqueFRT *) { --opaqueRefCount; }

OptionalOnlyFRT *OptionalOnlyFRT::create(int value) {
  return new OptionalOnlyFRT{value};
}

HoldsFRT makeHoldsFRT(SharedFRT *shared) { return {shared, 5}; }
