#import <Foundation/Foundation.h>

@interface Foo : NSObject
@end

Foo* getAFoo() {
    return [[Foo alloc] init];
}
