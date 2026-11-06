// RUN: rm -rf %t && split-file %s %t
// RUN: %clang_cc1 -std=c++2b -fsyntax-only -I %t -I %S/Inputs \
// RUN:     -flifetime-safety-inference -fexperimental-lifetime-safety-tu-analysis \
// RUN:     -Wlifetime-safety-experimental-field-origin-escape \
// RUN:     %t/test.cpp -verify=expected,common,ref,htmlspan -Wownership-inference

//--- WebCore/html/HTMLSpanElement.h
#pragma once
#include <WebCore/dom/Node.h>

namespace WebCore {

// expected-warning@+1 {{type 'WebCore::Element' was inferred as 'self-contained'}}
class Element : public ContainerNode { };
// expected-warning@+1 {{type 'WebCore::StyledElement' was inferred as 'self-contained'}}
class StyledElement : public Element { };
// expected-warning@+1 {{type 'WebCore::HTMLElement' was inferred as 'self-contained'}}
class HTMLElement : public StyledElement { };

class HTMLSpanElement final : public HTMLElement {
// expected-warning@-1 {{type 'WebCore::HTMLSpanElement' was inferred as 'self-contained'}}
public:
    ~HTMLSpanElement();
};

} // namespace WebCore

//--- test.cpp
#include <WebCore/html/HTMLSpanElement.h>
#include <wtf/Ref.h>

using namespace WebCore;

template <class T> T get();
auto force_instantiate = get<Ref<HTMLSpanElement>>();
