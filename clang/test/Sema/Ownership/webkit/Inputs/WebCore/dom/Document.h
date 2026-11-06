#pragma once

#include <WebCore/dom/Node.h>

namespace WebCore {

// common-warning@+1 {{type 'WebCore::Document' was inferred as 'self-contained'}}
class Document : public ContainerNode {
public:
    void ref() const final { ContainerNode::ref(); }
    void deref() const final { ContainerNode::deref(); }

    virtual ~Document();

    // Out-of-line in Document.cpp.
    void removedLastRef() __attribute__((ownership_takes(res, 1)));
};

} // namespace WebCore
