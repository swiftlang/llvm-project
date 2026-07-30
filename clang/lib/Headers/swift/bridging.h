//===--- bridging.h - Swift and [Obj]C[++] Interop ----------------*- C -*-===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2026 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//
//
// This file provides annotation for C, ObjC, and C++ code written to
// interoperate with Swift.
//
//===----------------------------------------------------------------------===//

#ifndef SWIFT_CLANGIMPORTER_SWIFT_INTEROP_SUPPORT_H
#define SWIFT_CLANGIMPORTER_SWIFT_INTEROP_SUPPORT_H

#ifdef __has_attribute
#define _CXX_INTEROP_HAS_ATTRIBUTE(x) __has_attribute(x)
#else
#define _CXX_INTEROP_HAS_ATTRIBUTE(x) 0
#endif

#if _CXX_INTEROP_HAS_ATTRIBUTE(swift_attr)

/// Specifies that a C++ class or struct owns and controls the lifetime of all
/// of the objects it references.
///
/// Such a type must not reference any objects whose
/// lifetime is controlled externally. This annotation lets Swift import methods
/// that return a `class` or `struct` type annotated with this macro.
///
/// For example, the following code lets Swift import `myMethod()`,
/// which returns `MyClass` by value:
/// ```c++
/// class SWIFT_SELF_CONTAINED MyClass {
/// public:
///     MyClass myMethod() const;
/// };
/// ```
#define SWIFT_SELF_CONTAINED __attribute__((swift_attr("import_owned")))

/// Specifies that a C/C++ method returns a value whose lifetime doesn't depend on
/// the object it is called on or on any of the method's parameters.
///
/// Apply this when the method's return value is self-contained, meaning it
/// borrows nothing from the object you call it on and nothing from any of its
/// parameters. It holds only when the result is genuinely independent, such as a
/// fresh owning value the method builds.
///
/// The macro lets Swift import a return-by-value that it otherwise treats as
/// unsafe, either refusing it or importing it conservatively. When the return type
/// is one the importer already accepts, such as a `std::string` the method builds,
/// the annotation only documents intent and doesn't change what Swift imports.
///
/// For example, Swift imports the following method, which returns a `std::string`:
/// ```c++
/// class MyClass {
///     std::string myMethod() const SWIFT_RETURNS_INDEPENDENT_VALUE;
/// };
/// ```
///
/// Usage in Swift:
/// ```swift
/// let object = MyClass()
/// print(String(object.myMethod()))
/// ```
#define SWIFT_RETURNS_INDEPENDENT_VALUE __attribute__((swift_attr("import_unsafe")))

#define _CXX_INTEROP_STRINGIFY(_x) #_x

#define _CXX_INTEROP_CONCAT_(a,b,c,d,e,f,g,i,j,k,l,m,n,o,p,...)         \
  #a "," #b "," #c "," #d "," #e "," #f "," #g "," #i "," #j "," #k "," \
  #l "," #m "," #n "," #o "," #p
#define _CXX_INTEROP_CONCAT(...) \
  _CXX_INTEROP_CONCAT_(__VA_ARGS__,,,,,,,,,,,,,,,,,)

/// Specifies that a C/C++ class or struct is reference-counted using
/// the given retain and release functions.
///
/// Swift imports the type as a `class` and manages its lifetime with automatic
/// reference counting (ARC). Rather than copying the value, Swift holds a
/// reference to it and calls the retain function when it takes a reference, and
/// the release function when it drops one. This fits a C++ type with shared,
/// dynamic ownership backed by a reference count, like an object managed by a
/// `std::shared_ptr`.
///
/// Declare the retain and release functions as free functions, each taking a
/// pointer to the type. Both must exist. Naming one that isn't declared is a
/// compile error.
///
/// A function that returns the type must say which ownership convention it uses,
/// either `SWIFT_RETURNS_RETAINED` or `SWIFT_RETURNS_UNRETAINED`. Because a C++
/// pointer can be null, a function that returns a pointer imports as an optional.
///
/// For example, Swift imports the following reference-counted C++ class as a
/// reference-counted type:
///  ```c++
///    class SWIFT_SHARED_REFERENCE(retainMyClass, releaseMyClass)
///    MyClass : IntrusiveReferenceCounted<MyClass> {
///    public:
///      static MyClass *create() SWIFT_RETURNS_RETAINED;
///      void myMethod();
///    };
///
///    void retainMyClass(MyClass *);
///    void releaseMyClass(MyClass *);
///  ```
///
/// Usage in Swift:
/// ```swift
/// // create() returns a pointer, so Swift imports it as an optional.
/// let object = MyClass.create()!
/// object.myMethod()
/// // The Swift compiler releases object here.
/// ```
#define SWIFT_SHARED_REFERENCE(_retain, _release)                          \
  __attribute__((swift_attr("import_reference")))                          \
  __attribute__((swift_attr(_CXX_INTEROP_STRINGIFY(retain:_retain))))      \
  __attribute__((swift_attr(_CXX_INTEROP_STRINGIFY(release:_release))))

/// Specifies that a C/C++ class or struct is a reference type whose lifetime
/// is presumed to be immortal.
///
/// This macro marks a type whose instance lives for the entire run of the
/// program, such as a singleton or a long-lived global owned by the C++ side.
/// Swift imports the type as a reference type (a `class`) and assumes its
/// instances never go away. Because nothing ever needs to be freed, Swift skips
/// its usual memory management: it performs no retain or release and treats the
/// reference as always valid.
///
/// How the type is returned decides whether Swift sees it as optional. A function
/// that returns it by reference, such as `static MyClass &shared()`, imports as a
/// non-optional `MyClass`, because a C++ reference can't be null. A function that
/// returns a pointer imports as an optional instead.
///
/// Even though the type imports as a `class`, Swift doesn't treat it as
/// class-constrained, so the identity operator `===` doesn't compile for it. To
/// confirm that two values refer to the same instance, change some shared state
/// through one value and read that change back through the other. Because both
/// point to a single underlying object, the update is visible either way.
///
/// For example, Swift imports the following singleton C++ class as a reference
/// type:
/// ```c++
/// class SWIFT_IMMORTAL_REFERENCE MyClass {
/// public:
///     static MyClass &shared();
///     void myMethod(int x);
/// };
/// ```
///
/// Usage in Swift:
/// ```swift
/// let object = MyClass.shared()
/// object.myMethod(123)
/// ```
#define SWIFT_IMMORTAL_REFERENCE                     \
  __attribute__((swift_attr("import_reference")))    \
  __attribute__((swift_attr("retain:immortal")))     \
  __attribute__((swift_attr("release:immortal")))

/// Specifies that a C/C++ class or struct is a reference type whose lifetime
/// isn't managed automatically.
///
/// Swift imports the type as a `class` with reference and identity semantics,
/// but performs no automatic reference counting on it. It neither retains nor
/// releases the object, so you must keep the underlying C++ instance alive for
/// as long as Swift holds a reference to it.
///
/// This macro flags the reference as unsafe instead of assuming the object
/// lives forever or managing its lifetime for you. It's the right choice only
/// when neither an immortal singleton nor a reference-counted object matches
/// your ownership model, and you accept responsibility for keeping the object
/// alive.
///
/// Because a C++ pointer can be null, a function that returns a pointer imports
/// as an optional. The imported type isn't class-constrained, so the identity
/// operator `===` doesn't compile for it. To check whether two values point to
/// the same object, change some shared state through one and read it back
/// through the other.
///
/// For example, Swift imports the following C++ class as an unsafe reference type:
/// ```c++
/// class SWIFT_UNSAFE_REFERENCE MyClass {
///     static MyClass *create();
///     std::string myMethod() const;
/// };
/// ```
///
/// Usage in Swift:
/// ```swift
/// // create() returns a pointer, so Swift imports it as an optional.
/// let object = MyClass.create()!
/// print(String(object.myMethod()))
/// ```
#define SWIFT_UNSAFE_REFERENCE                       \
  __attribute__((swift_attr("import_reference")))    \
  __attribute__((swift_attr("retain:immortal")))     \
  __attribute__((swift_attr("release:immortal")))    \
  __attribute__((swift_attr("unsafe")))

/// Makes a C++ smart pointer interchangeable with its underlying reference
/// type.
///
/// This annotation improves ergonomics by:
///
/// * introducing explicit conversions between the smart pointer type and
///   the underlying reference type and
/// * importing C++ APIs that return the smart pointer type or take it as a
///   parameter as returning or taking the underlying reference type.
///
/// Place this annotation on a smart pointer class or struct. The argument
/// to the annotation names an accessor function, which can be a file-scope
/// function or (if prefixed by .) a member function of the smart pointer
/// class. The return type of the accessor function determines the underlying
/// reference type of the smart pointer; it must be a pointer to a class or
/// struct with the `SWIFT_SHARED_REFERENCE` annotation. Note that
/// `SWIFT_SHARED_REFERENCE` requires the underlying reference type
/// to provide independent retain and release operations; Swift doesn't
/// currently support this ergonomic import of smart pointers to objects
/// that can only be managed through a smart pointer class.
///
/// The Swift importer diagnoses a class with this annotation as an
/// error if it can't determine an underlying reference type. If the class
/// is templated, this applies to instantiations of the template, not to the
/// template pattern. (That is, you can have a templated smart pointer
/// type as long as the concrete instantiations used in your C++ interface
/// all have determinable underlying reference types.)
///
/// The smart pointer type must have a constructor that takes a raw
/// pointer. This constructor performs a retain of the object.
/// (In the +0 / +1 language of Objective-C reference counting, it takes
/// the object at +0, so it doesn't take responsibility for a retain
/// performed by its caller.) Generally, this kind of constructor matches with
/// object models where the object constructor initializes the reference
/// count to 0.
///
/// The argument of this annotation must be the name of an "accessor" method
/// that returns a raw pointer to the object. This method returns a raw pointer
/// to the object without changing the reference count.
///
/// Swift uses these functions to convert between the raw-pointer and
/// smart-pointer representations.
///
/// By default, Swift assumes smart pointer types have a valid null state.
/// Passing a null pointer to the raw-pointer constructor creates a smart
/// pointer in the null state, and calling the raw-pointer accessor method
/// on a smart pointer in the null state returns a null pointer. If you annotate
/// the constructor and the raw-pointer accessor with `_Nonnull`, Swift assumes
/// the smart pointer doesn't have a null state.
///
/// ```c++
/// template <class T>
/// struct SWIFT_REFCOUNTED_PTR(.getPtr) MyPtr {
///     MyPtr(T* ptr);
///     T *_Nullable getPtr() const { return ptr; }
/// };
///
/// using MyClassPtr = MyPtr<MyClass>;
/// ```
///
/// In Swift, you convert the smart pointer type to the corresponding native
/// Swift reference:
/// ```swift
/// func f(_ x: MyClassPtr) {
///     let y = x.asReference
/// }
/// ```
///
/// Moreover, this annotation introduces implicit bridging for functions taking
/// or returning smart pointers by value. For example, Swift imports the
/// following code with the signature `func foo(_ param: MyClass) -> MyClass`:
/// ```c++
/// MyClassPtr foo(MyClassPtr param);
/// ```
#define SWIFT_REFCOUNTED_PTR(_toRawPointer)                                            \
  __attribute__((swift_attr(                                                           \
      "@_refCountedPtr(ToRawPointer: \"" _CXX_INTEROP_STRINGIFY(_toRawPointer) "\")")))

/// Specifies a name to use in Swift for this declaration instead of its original C/C++ name.
///
/// This macro changes only the name Swift imports; the C or C++ declaration
/// itself is untouched. You can rename types, members, and functions, and even
/// re-project a free function as a member of a Swift type.
///
/// The macro takes one of two argument shapes, chosen by the kind of declaration
/// you annotate. Types, fields, variables, and enum cases take a bare identifier,
/// such as `MyClass`. Functions and methods take the signature form, with
/// parentheses and argument labels, such as `add(first:second:)`.
///
/// Always include the parentheses on a function or method.
/// A bare identifier on a function doesn't work. Clang warns that the argument
/// must be a Swift function name, then discards the attribute, so Swift never
/// sees the new name.
///
/// The new name replaces the original. After a rename, the pre-rename name is
/// gone from Swift, and referring to it from Swift is an error.
///
/// You can also re-project a free function as a member of another type, turning
/// it into an instance method, an initializer, or a computed-property getter. The
/// `self:` label marks which parameter becomes the instance the method operates on.
///
/// For example, the following code renames a class and its member function,
/// labels the arguments of a free function, and re-projects a free function as a
/// method:
/// ```c++
/// // Rename the class with a bare identifier, and rename its member function
/// // with the signature form. Functions and methods need the parentheses.
/// class MyCxxClass {
/// public:
///     int myMethod() SWIFT_NAME(cxxMyMethod());
/// } SWIFT_NAME(MyClass);
///
/// // The signature form also assigns Swift argument labels to a free function.
/// int myAddNumbers(int a, int b) SWIFT_NAME(add(first:second:));
///
/// // Re-project a free function as a method on MyClass; `self:` marks the instance.
/// int myClassRank(MyClass c) SWIFT_NAME(MyClass.rank(self:));
/// ```
///
/// Usage in Swift:
/// ```swift
/// // The original C++ names are gone; Swift sees only the renamed declarations.
/// var object = MyClass()
/// object.cxxMyMethod()             // C++ myMethod()
/// print(object.rank())             // C++ free function myClassRank(_:)
/// print(add(first: 2, second: 3))  // C++ myAddNumbers(_:_:)
/// ```
#define SWIFT_NAME(_name) __attribute__((swift_name(#_name)))

/// Specifies that a specific C++ class or struct conforms to a specific
/// Swift protocol.
///
/// Swift imports a C++ class or struct as a plain type that conforms to no
/// protocols. This macro tells Swift to treat the imported type as conforming to
/// one of your Swift protocols. Once it does, you can use the type
/// anywhere that protocol is expected: pass it to a function that takes
/// `some MyProtocol`, or use it to satisfy a generic constraint like
/// `<T: MyProtocol>`. The conformance lives entirely on the Swift side, so
/// annotating the type changes nothing about how it compiles or behaves in C++.
///
/// The argument names the protocol as `ModuleName.ProtocolName`. Here `ModuleName`
/// is the Swift module that declares the protocol. This might be your own app or
/// library target. It is not the C++ module the type is imported from, and the two
/// are easy to confuse because they are almost always different.
///
/// For the conformance to hold, the type's imported members must supply what the
/// protocol requires. Swift matches each protocol requirement against the imported
/// Swift signatures of the type's members, so a member satisfies a requirement only
/// when its imported signature matches that requirement. For example, a requirement
/// `func identifier() -> Int32` is satisfied by a C++ member that imports as
/// `func identifier() -> Int32`.
///
/// For example, the following code conforms a C++ class to a protocol declared in
/// the Swift module `MyModule`. The imported member `identifier()` satisfies the
/// protocol's requirement:
/// ```c++
/// class SWIFT_CONFORMS_TO_PROTOCOL(MyModule.MyProtocol) MyClass {
/// public:
///     int identifier() const;
/// };
/// ```
///
/// Usage in Swift:
/// ```swift
/// // In MyModule: the protocol MyClass claims to satisfy.
/// protocol MyProtocol { func identifier() -> Int32 }
///
/// // MyClass conforms, so it satisfies the generic constraint `some MyProtocol`.
/// func use(_ value: some MyProtocol) -> Int32 { value.identifier() }
/// print(use(MyClass()))
/// ```
#define SWIFT_CONFORMS_TO_PROTOCOL(_moduleName_protocolName) \
  __attribute__((swift_attr(_CXX_INTEROP_STRINGIFY(conforms_to:_moduleName_protocolName))))

/// Imports a specific C++ method as a computed property.
///
/// If you apply this macro to a getter function, Swift synthesizes a getter. If you
/// apply it to both a getter and a setter function, Swift synthesizes a getter and a
/// setter.
///
/// Swift derives the property name by removing the `get` or `set` prefix from
/// the method name and lowercasing the first letter of the remaining text. For
/// example, `getValue` and `setValue` produce a property named `value`.
///
/// For a read-write property, the getter and setter must use the same text
/// after the prefix. If they don't match, such as `getValue` paired with
/// `setVal`, Swift imports a read-only property instead. The individual get
/// and set methods remain callable directly, but the computed property only
/// supports reading.
///
/// If a setter doesn't have a matching getter, Swift doesn't
/// create a computed property. Referring to the property name is
/// an error.
///
/// Avoid giving a private backing field the same name as the derived property.
/// For example, `getValue` and `setValue` derive the property name `value`; if
/// the class also has a private field named `value`, the macro won't create a computed property.
///
/// This macro doesn't currently support functions that pass values by reference.
///
/// For example, Swift imports the following C++ getter and setter together as a
/// read-write property, `var value: CInt { get set }`:
/// ```c++
/// class MyClass {
/// public:
///     int getValue() const SWIFT_COMPUTED_PROPERTY;
///     void setValue(int newValue) SWIFT_COMPUTED_PROPERTY;
/// };
/// ```
///
/// Usage in Swift:
/// ```swift
/// var object = MyClass()
/// // Calls `setValue()`.
/// object.value = 123
/// // Calls `getValue()`.
/// print(object.value)
/// ```
///
#define SWIFT_COMPUTED_PROPERTY \
  __attribute__((swift_attr("import_computed_property")))

/// Specifies that Swift imports a constant C++ member function as a
/// mutating Swift method.
///
/// Add this annotation to a constant C++ member function that mutates a
/// `mutable` field in a C++ object. Swift then treats the function as mutating
/// and imports it as a `mutating` method.
///
/// For example, Swift imports the following code as a
/// `mutating func myMethod()`:
/// ```c++
/// class MyClass {
///     mutable int cached;
/// public:
///     void myMethod() const SWIFT_MUTATING;
/// };
/// ```
#define SWIFT_MUTATING \
  __attribute__((swift_attr("mutating")))

/// Specifies that Swift imports a C/C++ class or struct as a type that is
/// safe to share across concurrent contexts.
///
/// Swift can mark some imported C++ types as `Sendable` on its own, but only when
/// it can verify that every stored member is itself safe to share. This macro
/// matters when a type holds members Swift can't verify, such as raw
/// pointers, `std::mutex`, `std::shared_ptr`, or other opaque C++ types.
/// Without the macro, the compiler flags the type and prevents it from
/// being shared across concurrent contexts.
///
/// Applying this macro tells Swift to treat the type as `Sendable` anyway,
/// skipping that verification. You're then responsible for ensuring the type is
/// actually safe to use concurrently.
///
/// For example, Swift imports the following code as
/// `struct MyClass: @unchecked Sendable`:
/// ```c++
/// class SWIFT_UNCHECKED_SENDABLE MyClass
/// { ... }
/// ```
///
/// Usage in Swift:
/// ```swift
/// // MyClass imports as @unchecked Sendable, so it satisfies a Sendable requirement.
/// let value: any Sendable = MyClass()
/// ```
#define SWIFT_UNCHECKED_SENDABLE \
  __attribute__((swift_attr("@Sendable")))

/// Specifies that Swift imports a C/C++ class or struct as a non-copyable
/// Swift value type.
///
/// A non-copyable value has a single owner. Swift makes no implicit copies of it,
/// and assigning or passing it consumes the source binding, so any later use of
/// that binding is an error. This suits move-only, resource-owning C++ types, such
/// as a class with a deleted copy constructor and defaulted move operations. The
/// annotation changes how Swift projects the type, not the C++ type itself.
///
/// For example, Swift imports the following code as
/// `struct MyStruct: ~Copyable`:
/// ```c++
/// struct SWIFT_NONCOPYABLE MyStruct { int x; };
/// ```
///
/// Usage in Swift:
/// ```swift
/// let a = MyStruct()
/// let b = a          // consumes a; using a afterward is an error
/// print(b.x)
/// ```
#define SWIFT_NONCOPYABLE \
  __attribute__((swift_attr("~Copyable")))

/// Specifies that Swift imports a C/C++ class or struct as a non-copyable
/// Swift value type that calls the given destroy function.
///
/// Swift imports the type as non-copyable and calls the named destroy function
/// when the value goes out of scope, so a C or C++ type that needs explicit
/// cleanup releases its resources deterministically.
///
/// For example, Swift imports the following C struct as a non-copyable value that
/// frees its members when it goes out of scope:
/// ```c
/// typedef struct SWIFT_NONCOPYABLE_WITH_DESTROY(destroyMyStruct) MyStruct {
///     void *storage;
/// } MyStruct;
///
/// void destroyMyStruct(MyStruct toBeDestroyed);
/// MyStruct createMyStruct(void);
/// ```
///
/// Usage in Swift:
/// ```swift
/// let mt = createMyStruct()
/// let mt2 = mt // consumes mt
/// // once mt2 is unused, Swift calls destroyMyStruct(mt2)
/// ```
#define SWIFT_NONCOPYABLE_WITH_DESTROY(_destroy) \
  __attribute__((swift_attr("~Copyable"))) \
  __attribute__((swift_attr(_CXX_INTEROP_STRINGIFY(destroy:_destroy))))

/// Specifies that Swift imports a C++ class or struct as a copyable
/// Swift value if all of the specified template arguments are copyable.
///
/// This macro propagates copyability through a class template instead of
/// declaring a fixed type copyable. An instantiation is copyable only when the
/// named template parameters are themselves copyable. Instantiating the template
/// with a non-copyable argument makes the whole instantiation non-copyable.
///
/// For example, given the following code, `MyStruct<int>` is imported as
/// copyable, while `MyStruct<NonCopyableType>` is imported as non-copyable:
/// ```c++
/// template <class T>
/// struct SWIFT_COPYABLE_IF(T) MyStruct { T value; };
/// ```
#define SWIFT_COPYABLE_IF(...) \
  __attribute__((swift_attr("copyable_if:" _CXX_INTEROP_CONCAT(__VA_ARGS__))))

/// Specifies that Swift imports a C/C++ class or struct
/// as a non-escapable Swift value type.
///
/// A non-escapable value can't outlive the scope it was created in. Swift won't
/// let it be stored in a property, returned from a function, or otherwise
/// escape the local scope. Use it for a C++ type that borrows memory it does
/// not own, such as `std::span`, `std::string_view`, or an iterator, so Swift
/// code can't accidentally hold on to it beyond the lifetime of the data it
/// points to.
///
/// This support is currently limited. Swift normally relaxes those rules for
/// functions with `@lifetime` annotations, but there is no C++ macro equivalent
/// for them. As a result, any function or method that returns this type
/// (including constructors and factory methods) must also be annotated with
/// `SWIFT_RETURNS_INDEPENDENT_VALUE`, or Swift won't import it.
///
/// For example, Swift imports the following struct as
/// `struct MyStruct: ~Escapable`. Because `create` returns `MyStruct` by value, it
/// carries `SWIFT_RETURNS_INDEPENDENT_VALUE` so Swift can call it:
/// ```c++
/// struct SWIFT_NONESCAPABLE MyStruct {
///     const int *data;
///     int count;
///
///     // Returns MyStruct by value, so Swift needs this annotation to call it.
///     static MyStruct create(const int *data, int count)
///         SWIFT_RETURNS_INDEPENDENT_VALUE;
/// };
/// ```
#define SWIFT_NONESCAPABLE \
  __attribute__((swift_attr("~Escapable")))

/// Specifies that Swift imports a C/C++ class or struct as a safe escapable
/// value in safe interop mode.
///
/// An escapable value is one that can be freely stored and returned because it
/// owns its data rather than borrowing it. Every type is escapable by default,
/// so in normal interop mode this annotation has no effect and only documents
/// that intent.
///
/// The annotation earns its keep when the compiler is being cautious and flags
/// a C++ type as unsafe. Applying it opts the type into the safe, escapable
/// category, so the compiler doesn't flag it.
///
/// For example, the following code ensures `MyStruct` isn't marked as unsafe
/// in safe interop mode:
/// ```c++
/// struct SWIFT_ESCAPABLE MyStruct { int x; };
/// ```
///
/// Usage in Swift:
/// ```swift
/// // MyStruct is escapable, so it can bind to a `let` and be stored freely.
/// var values: [MyStruct] = []
/// values.append(MyStruct())
/// ```
#define SWIFT_ESCAPABLE \
  __attribute__((swift_attr("Escapable")))

/// Specifies that Swift imports a C++ class or struct as an escapable
/// Swift value if all of the specified template arguments are escapable.
///
/// Rather than declaring a fixed type escapable, this macro makes a class
/// template's escapability depend on its type arguments. The imported type is
/// escapable only when the named arguments are escapable; build it with a
/// non-escapable argument and the result is non-escapable too.
///
/// Because the macro applies to a template, Swift needs a concrete type to
/// import. Provide one with a `using` alias such as `MyStruct<int>`; Swift can't
/// import the bare template on its own.
///
/// For example, given the following code, `MyStruct<T>` is imported as escapable
/// when `T` is escapable:
/// ```c++
/// template <class T>
/// struct SWIFT_ESCAPABLE_IF(T) MyStruct {
///     MyStruct(T value) : value(value) {}
///     T value;
/// };
///
/// // Swift imports the concrete type, not the template itself.
/// using MyIntStruct = MyStruct<int>;
/// ```
///
/// Usage in Swift:
/// ```swift
/// // MyStruct<int> is escapable, so the value can bind to a `let` and escape.
/// let value = MyIntStruct(11)
/// ```
#define SWIFT_ESCAPABLE_IF(...) \
  __attribute__((swift_attr("escapable_if:" _CXX_INTEROP_CONCAT(__VA_ARGS__))))

/// Specifies that a C/C++ function or method returns an owned reference-counted
/// value.
///
/// Apply this to a function or method that returns a type annotated with
/// `SWIFT_SHARED_REFERENCE`. Swift receives the value already retained, at +1, and
/// takes ownership of it. Swift emits the balancing release when the
/// binding goes out of scope, and adds no extra retain at the hand-off. It fits
/// `create`, `make`, or `copy` style functions that allocate the object, so the
/// object starts life with a reference count of one.
///
/// Because a C++ pointer can be null, a function that returns a pointer imports as
/// an optional.
///
/// For example, the following function hands back a retained value that Swift owns:
/// ```c++
/// class SWIFT_SHARED_REFERENCE(retainMyClass, releaseMyClass)
/// MyClass {
/// public:
///     static MyClass *create() SWIFT_RETURNS_RETAINED;
/// };
/// ```
///
/// Usage in Swift:
/// ```swift
/// // create() returns a pointer, so Swift imports it as an optional.
/// // Swift owns object and releases it when the binding goes out of scope.
/// let object = MyClass.create()!
/// ```
#define SWIFT_RETURNS_RETAINED __attribute__((swift_attr("returns_retained")))
/// Specifies that a C/C++ function or method returns an unowned reference-counted
/// value.
///
/// Apply this to a function or method that returns a type annotated with
/// `SWIFT_SHARED_REFERENCE`. Swift receives the value without an extra
/// retain, at +0, and doesn't take ownership of it. Swift doesn't release it,
/// because something else owns it, typically a cached, process-lifetime singleton.
/// It fits `get`, `shared`, or `find` style accessors that return a
/// reference to an object they don't own.
///
/// Because a C++ pointer can be null, a function that returns a pointer imports as
/// an optional.
///
/// For example, the following accessor returns an unowned value that Swift doesn't release:
/// ```c++
/// class SWIFT_SHARED_REFERENCE(retainMyClass, releaseMyClass)
/// MyClass {
/// public:
///     static MyClass *shared() SWIFT_RETURNS_UNRETAINED;
/// };
/// ```
///
/// Usage in Swift:
/// ```swift
/// // shared() returns a pointer, so Swift imports it as an optional.
/// // Swift doesn't release object; its owner keeps it alive.
/// let object = MyClass.shared()!
/// ```
#define SWIFT_RETURNS_UNRETAINED                                               \
  __attribute__((swift_attr("returns_unretained")))

/// Specifies that a reference-counted C/C++ type is returned as an unowned value
/// by default.
///
/// Apply this to a foreign reference type annotated with
/// `SWIFT_SHARED_REFERENCE`. APIs returning the type are then assumed to
/// return an unowned (+0) value by default, unless a specific API is
/// explicitly annotated with `SWIFT_RETURNS_RETAINED`.
///
/// For example, given the following code, Swift assumes APIs returning `MyClass*`
/// return an unowned value:
/// ```c++
/// struct SWIFT_SHARED_REFERENCE(retainMyClass, releaseMyClass)
/// SWIFT_RETURNED_AS_UNRETAINED_BY_DEFAULT
/// MyClass { ... };
/// ```
#define SWIFT_RETURNED_AS_UNRETAINED_BY_DEFAULT                                    \
  __attribute__((swift_attr("returned_as_unretained_by_default")))

/// Specifies that the non-public members of a C++ class, struct, or union can
/// be accessed from extensions of that type.
///
/// A C++ type's private and protected members are normally invisible to Swift.
/// This macro opens them to a single Swift file that you name, as if those members
/// were declared private in that file. The type's internals stay hidden from the
/// rest of your Swift code, so you reach them in just one place.
///
/// The argument is the file's path in the form `"ModuleName/FileName.swift"`.
/// `ModuleName` is the Swift module the file compiles into, which might be your
/// target or product name, not the C++ (Clang) module the type is imported from.
///
/// Two conditions must hold for the access to work. First, the code must sit in a
/// Swift `extension` of the imported type. A free function in the same file still
/// can't see the private members. Second, the file name must match exactly, so
/// naming a different file makes Swift deny the access.
///
/// For example, this annotation lets extensions in `MySwiftModule/MySwiftFile.swift`
/// reach the private members of `MyCxxClass`:
///
/// ```c++
/// class SWIFT_PRIVATE_FILEID("MySwiftModule/MySwiftFile.swift")
/// MyCxxClass {
/// private:
///     void privateMethod();
///     int privateStorage;
/// };
/// ```
///
/// Usage in Swift:
///
/// ```swift
/// // MySwiftModule/MySwiftFile.swift
/// extension MyCxxClass {
///     func ext() {
///         privateMethod()
///         print("\(privateStorage)")
///     }
/// }
/// ```
#define SWIFT_PRIVATE_FILEID(_fileID) \
  __attribute__((swift_attr("private_fileid:" _fileID)))

/// Specifies that Swift imports a C/C++ type or function as an unsafe
/// declaration.
///
/// Using these declarations triggers a warning in
/// strictly memory-safe Swift. Use the `unsafe` keyword to suppress
/// these warnings.
///
/// For example, given the following code, calling `myFunction()` triggers a
/// warning in strictly memory-safe Swift:
/// ```c++
/// void *myFunction() SWIFT_UNSAFE;
/// ```
#define SWIFT_UNSAFE __attribute__((swift_attr("unsafe")))

/// Specifies that a C/C++ type or function is considered safe.
///
/// Swift considers such declarations safe even if they have
/// unsafe constituents.
///
/// For example, given the following code, `myFunction()` is considered safe
/// even though it touches unsafe constituents:
/// ```c++
/// int myFunction(const MyClass &c) SWIFT_SAFE;
/// ```
#define SWIFT_SAFE __attribute__((swift_attr("safe")))

/// Suppresses generation of a safe wrapper overload for a C/C++ function even when
/// its annotations otherwise trigger one.
///
/// For example, the following code suppresses generation of a safe wrapper
/// overload for `myFunction()`:
/// ```c++
/// void myFunction(const int *ptr, size_t count) SWIFT_NO_SAFE_WRAPPER;
/// ```
#define SWIFT_NO_SAFE_WRAPPER __attribute__((swift_attr("no_safe_wrapper")))

#else  // #if _CXX_INTEROP_HAS_ATTRIBUTE(swift_attr)

// Empty defines for compilers that don't support `attribute(swift_attr)`.
#define SWIFT_SELF_CONTAINED
#define SWIFT_RETURNS_INDEPENDENT_VALUE
#define SWIFT_SHARED_REFERENCE(_retain, _release)
#define SWIFT_IMMORTAL_REFERENCE
#define SWIFT_UNSAFE_REFERENCE
#define SWIFT_REFCOUNTED_PTR(_toRawPointer)
#define SWIFT_NAME(_name)
#define SWIFT_CONFORMS_TO_PROTOCOL(_moduleName_protocolName)
#define SWIFT_COMPUTED_PROPERTY
#define SWIFT_MUTATING
#define SWIFT_UNCHECKED_SENDABLE
#define SWIFT_NONCOPYABLE
#define SWIFT_NONCOPYABLE_WITH_DESTROY(_destroy)
#define SWIFT_COPYABLE_IF(...)
#define SWIFT_NONESCAPABLE
#define SWIFT_ESCAPABLE
#define SWIFT_ESCAPABLE_IF(...)
#define SWIFT_RETURNS_RETAINED
#define SWIFT_RETURNS_UNRETAINED
#define SWIFT_RETURNED_AS_UNRETAINED_BY_DEFAULT
#define SWIFT_PRIVATE_FILEID(_fileID)
#define SWIFT_UNSAFE
#define SWIFT_SAFE
#define SWIFT_NO_SAFE_WRAPPER

#endif // #if _CXX_INTEROP_HAS_ATTRIBUTE(swift_attr)

#undef _CXX_INTEROP_HAS_ATTRIBUTE

#endif // SWIFT_CLANGIMPORTER_SWIFT_INTEROP_SUPPORT_H
