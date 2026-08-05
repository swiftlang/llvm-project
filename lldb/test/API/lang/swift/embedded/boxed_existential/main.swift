// A class-constrained existential stores the instance pointer directly; an
// opaque one has an existential container, whose payload is stored inline only
// if it fits in three words.
protocol ClassBound: AnyObject {
}

protocol P {
}

// Four words, so this does not fit in the existential's inline buffer and is
// boxed on the heap.
struct Boxed: P {
    let a = 111
    let b = 222
    let c = 333
    let d = 444
}

// Two words, so this is stored inline.
struct Inline: P {
    let a = 555
    let b = 666
}

// A class reference is one word, and is therefore never boxed.
class C: P, ClassBound {
    let classField = 777
}

enum E: P {
    case small(Int)
    case large(Int, Int, Int, Int)
}

// An error existential is a pointer to a heap box that holds the payload's type
// metadata, rather than an existential container.
struct MyError: Error {
    let code = 999
    let extra = 111
}

// A class payload is stored in the box as a reference, not inline.
class MyClassError: Error {
    let classCode = 4242
}

func mayThrow() throws {
    throw MyError()
}

func mayThrowClass() throws {
    throw MyClassError()
}

func f() {
    let pBoxed: any P = Boxed()
    let pInline: any P = Inline()
    let pClass: any P = C()
    let classBound: any ClassBound = C()
    // Tuples are not nominal, so the metadata the existential points at carries
    // no layout for them; DWARF is the only description of these.
    let boxedTuple: Any = (1, 2, 3, 4)
    let inlineTuple: Any = (5, 6)
    // FIXME: rdar://168697959 (Debug info for enums is not generated, if there
    // are no variables/parameter with the enum type in embedded swift)
    let bug = E.small(0)
    let pEnumSmall: any P = E.small(888)
    let pEnumLarge: any P = E.large(1, 2, 3, 4)
    do {
        try mayThrow()
    } catch {
        // Binding the error to a variable of its own gives that variable a
        // slot holding the box pointer. A direct `any Error = MyError()`
        // would instead be described as living inside the box.
        let anyError: any Error = error
        do {
            try mayThrowClass()
        } catch {
            let classError: any Error = error
            print("break here")
            _ = (anyError, classError)
        }
    }
}

f()
