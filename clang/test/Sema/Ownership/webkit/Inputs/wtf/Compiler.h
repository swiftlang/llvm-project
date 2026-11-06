#pragma once

#define ALWAYS_INLINE inline

#define LIFETIME_BOUND [[clang::lifetimebound]]

#define WTF_MAKE_NONCOPYABLE(Name) \
    Name(const Name&) = delete; \
    Name& operator=(const Name&) = delete;

#define _SWIFT_STRINGIFY(x) #x

#define SWIFT_SELF_CONTAINED __attribute__((swift_attr("Escapable")))

#define SWIFT_RETURNED_AS_UNRETAINED_BY_DEFAULT \
    __attribute__((swift_attr("returned_as_unretained_by_default")))

#define SWIFT_SHARED_REFERENCE(_retain, _release) \
    __attribute__((swift_attr("import_reference"))) \
    __attribute__((swift_attr(_SWIFT_STRINGIFY(retain:_retain)))) \
    __attribute__((swift_attr(_SWIFT_STRINGIFY(release:_release))))

#define BASE_CLASS_SWIFT_SHARED_REFERENCE(_retain, _release) \
    SWIFT_SHARED_REFERENCE(_retain, _release)
