#include "types.h"

int numConstructed = 0;
int numCopied = 0;
int numDestroyed = 0;
int numMoved = 0;

CopyCounted::CopyCounted(int value) : value(value) { ++numConstructed; }
CopyCounted::CopyCounted(const CopyCounted &other) : value(other.value) {
  ++numCopied;
}
CopyCounted::~CopyCounted() { ++numDestroyed; }

NonCopyable::NonCopyable(NonCopyable &&other) : x(other.x) {
  other.x = -123;
  ++numMoved;
}
