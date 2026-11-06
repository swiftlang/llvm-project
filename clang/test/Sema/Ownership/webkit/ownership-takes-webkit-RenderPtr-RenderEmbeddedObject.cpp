// RUN: rm -rf %t && split-file %s %t
// RUN: %clang_cc1 -std=c++2b -fsyntax-only -I %t -I %S/Inputs \
// RUN:     -flifetime-safety-inference -fexperimental-lifetime-safety-tu-analysis \
// RUN:     -Wlifetime-safety-experimental-field-origin-escape \
// RUN:     %t/test.cpp -verify=expected,renderptr -Wownership-inference

//--- wtf/Compiler.h
#pragma once

//--- WebCore/rendering/RenderObject.h
#pragma once

namespace WebCore {

class RenderObject {
// expected-warning@-1 {{type 'WebCore::RenderObject' was trivially inferred as 'self-contained'}}
public:
    // Out-of-line in RenderObject.cpp; consumes `*this` (it deletes the object).
    void destroy() __attribute__((ownership_takes(res, 1)));
};

// expected-warning@+1 {{type 'WebCore::RenderElement' was trivially inferred as 'self-contained'}}
class RenderElement : public RenderObject { };
// expected-warning@+1 {{type 'WebCore::RenderLayerModelObject' was trivially inferred as 'self-contained'}}
class RenderLayerModelObject : public RenderElement { };
// expected-warning@+1 {{type 'WebCore::RenderBoxModelObject' was trivially inferred as 'self-contained'}}
class RenderBoxModelObject : public RenderLayerModelObject { };
// expected-warning@+1 {{type 'WebCore::RenderBox' was trivially inferred as 'self-contained'}}
class RenderBox : public RenderBoxModelObject { };
// expected-warning@+1 {{type 'WebCore::RenderReplaced' was trivially inferred as 'self-contained'}}
class RenderReplaced : public RenderBox { };
// expected-warning@+1 {{type 'WebCore::RenderWidget' was trivially inferred as 'self-contained'}}
class RenderWidget : public RenderReplaced { };

} // namespace WebCore

//--- WebCore/rendering/RenderPtr.h
#pragma once
#include <memory>

namespace WebCore {

class RenderObject;

struct RenderObjectDeleter {
// expected-warning@-1 {{type 'WebCore::RenderObjectDeleter' was trivially inferred as 'self-contained'}}
    // Out-of-line in RenderObject.cpp; its body is `renderer->destroy();`, so it
    // consumes its RenderObject* argument (source index 2; index 1 is the
    // deleter object's own `this`).
    void operator()(RenderObject*) const __attribute__((ownership_takes(res, 2)));
};

template<typename T> using RenderPtr = std::unique_ptr<T, RenderObjectDeleter>;

} // namespace WebCore

//--- WebCore/rendering/RenderEmbeddedObject.h
#pragma once
#include <WebCore/rendering/RenderObject.h>

namespace WebCore {

// expected-warning@+1 {{type 'WebCore::RenderEmbeddedObject' was trivially inferred as 'self-contained'}}
class RenderEmbeddedObject final : public RenderWidget { };

} // namespace WebCore

//--- test.cpp
#include <WebCore/rendering/RenderEmbeddedObject.h>
#include <WebCore/rendering/RenderPtr.h>

using namespace WebCore;

template <class T> T get();
auto force_instantiate = get<RenderPtr<RenderEmbeddedObject>>();
