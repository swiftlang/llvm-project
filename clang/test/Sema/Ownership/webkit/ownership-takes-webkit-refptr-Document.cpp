// RUN: rm -rf %t && split-file %s %t
// RUN: %clang_cc1 -std=c++2b -fsyntax-only -I %t -I %S/Inputs \
// RUN:     -flifetime-safety-inference -fexperimental-lifetime-safety-tu-analysis \
// RUN:     -Wlifetime-safety-experimental-field-origin-escape \
// RUN:     %t/test.cpp -verify=expected,common,refptr -Wownership-inference

//--- test.cpp
#include <WebCore/dom/Document.h>
#include <wtf/RefPtr.h>

using namespace WebCore;

template <class T> T get();
auto force_instantiate = get<RefPtr<Document>>();
