#import <Foundation/Foundation.h>

#import "foo.h"

@interface Foo : NSObject
@end

@implementation Foo
@end

Foo* getAFoo() {
    return [[Foo alloc] init];
}
