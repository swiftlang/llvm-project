#pragma clang system_header

void * __sized_by_or_null(size) explicitly_sized(int size) __attribute__((alloc_size(1))); // ok, explicit and implicit bounds align

void * __sized_by(size)
    explicitly_sized_nonnull(int size)
    __attribute__((alloc_size(1)));

void * __sized_by(size) _Nonnull explicitly_sized_nonnull_silence(int size) __attribute__((alloc_size(1)));
void * __sized_by(size) explicitly_sized_returns_nonnull_silence(int size) __attribute__((returns_nonnull)) __attribute__((alloc_size(1)));
void * __sized_by(size) explicitly_sized_returns_nonnull_after_silence(int size) __attribute__((alloc_size(1))) __attribute__((returns_nonnull));

void * __sized_by_or_null(size)
// expected-note@-1{{previous attribute is here}}
    __sized_by(size)
// expected-error@-1{{pointer cannot have more than one count attribute}}
// expected-note@-2{{conflicting attributes were '__sized_by_or_null(size)' and '__sized_by(size)'}}
    _Nonnull
    explicitly_sized_nonnull_redundant(int size)
    __attribute__((alloc_size(1)));

char * __sized_by(size)
// expected-note@-1{{previous attribute is here}}
    _Nonnull
    __counted_by(size)
// expected-error@-1{{pointer cannot have more than one count attribute}}
// expected-note@-2{{conflicting attributes were '__sized_by(size)' and '__counted_by(size)'}}
    explicitly_sized_nonnull_redundant2(int size)
    __attribute__((alloc_size(1)));

char * __counted_by_or_null(size)
// expected-note@-1{{previous attribute is here}}
    explicitly_counted(int size)
    __attribute__((alloc_size(1)));
// expected-error@-1{{pointer cannot have more than one count attribute}}
// expected-note@-2{{conflicting attributes were '__counted_by_or_null(size)' and '__sized_by_or_null(size)'}}
// expected-note@-3{{function return type implicitly __sized_by_or_null because of the function's 'alloc_size' attribute}}

char * __counted_by(size)
// expected-note@-1{{previous attribute is here}}
    explicitly_counted_returns_nonnull(int size)
    __attribute__((alloc_size(1))) __attribute__((returns_nonnull));
// expected-error@-1{{pointer cannot have more than one count attribute}}
// expected-note@-2{{conflicting attributes were '__counted_by(size)' and '__sized_by(size)'}}
// expected-note@-3{{function return type implicitly __sized_by because of the function's 'alloc_size' and 'returns_nonnull' attributes}}

char * __counted_by(size)
// expected-note@-1{{previous attribute is here}}
    explicitly_counted_nonnull(int size)
    __attribute__((alloc_size(1)));
// expected-error@-1{{pointer cannot have more than one count attribute}}
// expected-note@-2{{conflicting attributes were '__counted_by(size)' and '__sized_by_or_null(size)'}}
// expected-note@-3{{function return type implicitly __sized_by_or_null because of the function's 'alloc_size' attribute}}


void * __sized_by_or_null(size * count) explicitly_sized_with_count(int size, int count) __attribute__((alloc_size(1, 2))); // ok, explicit and implicit bounds align

void * __sized_by_or_null(count * size)
// expected-note@-1{{previous attribute is here}}
    explicitly_sized_with_count_flipped(int size, int count)
    __attribute__((alloc_size(1, 2))); // unfortunate, but too much complexity to support
// expected-error@-1{{pointer cannot have more than one count attribute}}
// expected-note@-2{{conflicting attributes were '__sized_by_or_null(count * size)' and '__sized_by_or_null(size * count)'}}
// expected-note@-3{{function return type implicitly __sized_by_or_null because of the function's 'alloc_size' attribute}}

void * __sized_by_or_null(size * count)
// expected-note@-1{{previous attribute is here}}
    explicitly_sized_with_count_flipped2(int size, int count)
    __attribute__((alloc_size(2, 1))); // unfortunate, but too much complexity to support
// expected-error@-1{{pointer cannot have more than one count attribute}}
// expected-note@-2{{conflicting attributes were '__sized_by_or_null(size * count)' and '__sized_by_or_null(count * size)'}}
// expected-note@-3{{function return type implicitly __sized_by_or_null because of the function's 'alloc_size' attribute}}

void * __sized_by(size * count)
    explicitly_sized_nonnull_with_count(int size, int count)
    __attribute__((alloc_size(1, 2)));

void * __sized_by(size * count) _Nonnull explicitly_sized_nonnull_with_count_silence(int size, int count) __attribute__((alloc_size(1, 2)));
void * __sized_by(size * count) explicitly_sized_returns_nonnull_with_count_silence(int size, int count) __attribute__((returns_nonnull)) __attribute__((alloc_size(1, 2)));

char * __counted_by_or_null(size * count)
// expected-note@-1{{previous attribute is here}}
    explicitly_counted_with_count(int size, int count)
    __attribute__((alloc_size(1, 2)));
// expected-error@-1{{pointer cannot have more than one count attribute}}
// expected-note@-2{{conflicting attributes were '__counted_by_or_null(size * count)' and '__sized_by_or_null(size * count)'}}
// expected-note@-3{{function return type implicitly __sized_by_or_null because of the function's 'alloc_size' attribute}}

char * __counted_by_or_null(count)
// expected-note@-1{{previous attribute is here}}
    explicitly_counted_with_count2(int size, int count)
    __attribute__((alloc_size(1, 2))); // no guarantee that size is 1
// expected-error@-1{{pointer cannot have more than one count attribute}}
// expected-note@-2{{conflicting attributes were '__counted_by_or_null(count)' and '__sized_by_or_null(size * count)'}}
// expected-note@-3{{function return type implicitly __sized_by_or_null because of the function's 'alloc_size' attribute}}

char * __counted_by(size * count)
// expected-note@-1{{previous attribute is here}}
    explicitly_counted_nonnull_with_count(int size, int count)
    __attribute__((alloc_size(1, 2)));
// expected-error@-1{{pointer cannot have more than one count attribute}}
// expected-note@-2{{conflicting attributes were '__counted_by(size * count)' and '__sized_by_or_null(size * count)'}}
// expected-note@-3{{function return type implicitly __sized_by_or_null because of the function's 'alloc_size' attribute}}

__attribute__((alloc_size(1)))
    void * __sized_by(size)
    leading_attr(unsigned size) {
    return (void*)0;
}

__attribute__((alloc_size(1))) void * __sized_by(size) _Nonnull leading_attr_silence(unsigned size) {
    return (void*)0;
}

__attribute__((alloc_size(1))) void * __sized_by(size) leading_attr_silence_returns_nonnull(unsigned size) __attribute__((returns_nonnull)) {
    return (void*)0;
}

__attribute__((alloc_size(1)))
    // expected-error@-1{{pointer cannot have more than one count attribute}}
    // expected-note@-2{{conflicting attributes were '__counted_by(size)' and '__sized_by_or_null(size)'}}
    // expected-note@-3{{function return type implicitly __sized_by_or_null because of the function's 'alloc_size' attribute}}
    char * __counted_by(size)
    // expected-note@-1{{previous attribute is here}}
    leading_attr_clash(unsigned size) {
    return (void*)0;
}

void * unnamed_param(unsigned) __attribute__((alloc_size(1)));

void * unnamed_param_with_count(unsigned, unsigned) __attribute__((alloc_size(1, 2)));
__attribute__((alloc_size(1, 2))) void * unnamed_param_with_count(unsigned, unsigned) {
    return (void*)0;
}

void * inherit_attr(unsigned size) __attribute__((alloc_size(1)));

void * unnamed_param_inherit_attr(unsigned) __attribute__((alloc_size(1)));

void * unnamed_param_with_count_flipped(unsigned, unsigned)
    // expected-note@+1{{attribute inherited from previous declaration here}}
    __attribute__((alloc_size(1,2)));

void * unnamed_param_with_count_mismatching_attrs(unsigned, unsigned)
    // expected-note@+1{{conflicting attribute is here}}
    __attribute__((alloc_size(1,2)));

void * unnamed_param_with_count_mismatching_attrs_after_param_list(unsigned, unsigned)
    // expected-note@+1{{conflicting attribute is here}}
    __attribute__((alloc_size(1,2)));

// expected-note@+1{{conflicting attribute is here}}
__attribute__((alloc_size(1, 2)))
void * clashing_attrbutes_on_same_decl(unsigned, unsigned)
    // expected-error@+1{{'alloc_size' attribute does not match previous declaration}}
    __attribute__((alloc_size(2, 1)));

void * override_nullability(unsigned, unsigned)
    __attribute__((alloc_size(1, 2)));

void * override_nullability2(unsigned, unsigned)
    __attribute__((returns_nonnull))
    __attribute__((alloc_size(1, 2)));
