// RUN: rm -rf %t && split-file %s %t
// RUN: %clang_cc1 -std=c++2b -fsyntax-only -I %t -I %S/Inputs \
// RUN:     -flifetime-safety-inference -fexperimental-lifetime-safety-tu-analysis \
// RUN:     -Wlifetime-safety-experimental-field-origin-escape \
// RUN:     %t/test.cpp -verify=expected,common,ref,domparser -Wownership-inference

//--- WebCore/xml/DOMParser.h
#pragma once
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class Document;
class Settings;

// Non-essential members of the real DOMParser (the WeakPtr context-document
// field and parseFromString) are omitted; they are irrelevant to the
// Ref<DOMParser> destructor. ~DOMParser(), create(), and the constructor are
// declared but not defined, exactly as in the real header (bodies live in the
// .cpp), so the analysis does not see their bodies.
class DOMParser : public RefCounted<DOMParser> {
// expected-warning@-1 {{type 'WebCore::DOMParser' was inferred as 'self-contained'}}
public:
    static Ref<DOMParser> create(Document&);
    ~DOMParser();

private:
    explicit DOMParser(Document&);

    const Ref<const Settings> m_settings;
};

}

//--- test.cpp
#include <WebCore/xml/DOMParser.h>

using namespace WebCore;

template <class T> T get();
auto force_instantiate = get<Ref<DOMParser>>();
