#include "llvm/CAS/CASID.h"
#include "llvm/CAS/ObjectStore.h"

namespace llvm {
namespace cas {

struct Node {
  unsigned Code;
  unsigned ID;
  SmallString<256> Data;
  BitstreamObjectProxy CASRef;
}

} // namespace cas
} // namespace llvm