#import <Foundation/Foundation.h>

// A Clang swift_newtype wrapper. Unlike a plain typedef or a Swift typealias,
// the compiler preserves this __C type alias verbatim in the mangled name that
// identifies a type.
typedef NSString *MyAliasedKey __attribute__((swift_newtype(struct)));
