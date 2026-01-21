// REQUIRES: platform-xros

// RUN: %clang_cc1 -triple arm64-apple-xros1 -verify=ios -isysroot %S/Inputs/XROS.sdk %s 2>&1
// RUN: %clang_cc1 -triple arm64-apple-xros1 -fapplication-extension -verify=ios,ext -isysroot %S/Inputs/XROS.sdk %s 2>&1

// RUN: %clang_cc1 -triple arm64-apple-xros2 -DXROS2 -verify=ios -isysroot %S/Inputs/XROS.sdk  %s 2>&1

__attribute__((availability(ios, unavailable)))
void ios_unavail(); // ios-note {{}}

__attribute__((availability(ios_app_extension, unavailable)))
void ios_ext_unavail(); // ext-note {{}}

void use() {
  ios_unavail(); // ios-error {{'ios_unavail' is unavailable: not available on }}
  ios_ext_unavail(); // ext-error {{'ios_ext_unavail' is unavailable: not available on }}
}

__attribute__((availability(ios, introduced=10)))
void ios_introduced_10();

__attribute__((availability(ios_app_extension, introduced=10)))
void ios_ext_introduced_10();

__attribute__((availability(ios, introduced=15)))
void ios_introduced_15();

__attribute__((availability(ios_app_extension, introduced=15)))
void ios_ext_introduced_15();

__attribute__((availability(ios, introduced=16)))
void ios_introduced_16(); // ios-note {{}}

__attribute__((availability(ios_app_extension, introduced=16)))
void ios_ext_introduced_16(); // ext-note {{}}

void useIntroduced() {
  // introduced iOS < 10 => introduced xrOS 1
  ios_introduced_10();
  ios_ext_introduced_10();
  // introduced iOS 15 => introduced xrOS 1
  ios_introduced_15();
  ios_ext_introduced_15();
  // introduced iOS 16 => xros unavailable (no mapping)
  ios_introduced_16(); // ios-error {{is unavailable: not available on }}
  ios_ext_introduced_16(); // ext-error {{is unavailable: not available on }}
}

__attribute__((availability(ios, deprecated=10)))
void ios_deprecated_10(); // ios-note {{}}

__attribute__((availability(ios_app_extension, deprecated=10)))
void ios_ext_deprecated_10(); // ext-note {{}}

__attribute__((availability(ios, deprecated=15)))
void ios_deprecated_15(); // ios-note {{}}

__attribute__((availability(ios_app_extension, deprecated=15)))
void ios_ext_deprecated_15(); // ext-note {{}}

__attribute__((availability(ios, deprecated=16)))
void ios_deprecated_16();
#ifdef XROS2
// ios-note@-2 {{}}
#endif

__attribute__((availability(ios_app_extension, deprecated=16)))
void ios_ext_deprecated_16();

void useDeprecated() {
  // deprecated iOS < 10 => deprecated xrOS 1
  ios_deprecated_10(); // ios-warning {{is deprecated: first deprecated in}}
  ios_ext_deprecated_10(); // ext-warning {{is deprecated: first deprecated in}}
  // deprecated iOS 15 => deprecated xrOS 1
  ios_deprecated_15(); // ios-warning {{is deprecated: first deprecated in}}
  ios_ext_deprecated_15(); // ext-warning {{is deprecated: first deprecated in}}
  // deprecated iOS 16 => deprecated xrOS 1.0.99
  ios_deprecated_16();
#ifdef XROS2
  // ios-warning@-2 {{is deprecated: first deprecated in}}
#endif
  ios_ext_deprecated_16();
}

__attribute__((availability(ios, obsoleted=10)))
void ios_obsoleted_10(); // ios-note {{}}

__attribute__((availability(ios_app_extension, obsoleted=10)))
void ios_ext_obsoleted_10(); // ext-note {{}}

__attribute__((availability(ios, obsoleted=15)))
void ios_obsoleted_15(); // ios-note {{}}

__attribute__((availability(ios_app_extension, obsoleted=15)))
void ios_ext_obsoleted_15(); // ext-note {{}}

__attribute__((availability(ios, obsoleted=16)))
void ios_obsoleted_16();
#ifdef XROS2
// ios-note@-2 {{}}
#endif

__attribute__((availability(ios_app_extension, obsoleted=16)))
void ios_ext_obsoleted_16();

void useObsoleted() {
  // deprecated iOS < 10 => deprecated xrOS 1
  ios_obsoleted_10(); // ios-error {{is unavailable: obsoleted in}}
  ios_ext_obsoleted_10(); // ext-error {{is unavailable: obsoleted in}}
  // deprecated iOS 15 => deprecated xrOS 1
  ios_obsoleted_15(); // ios-error {{is unavailable: obsoleted in}}
  ios_ext_obsoleted_15(); // ext-error {{is unavailable: obsoleted in}}
  // obsoleted iOS 16 => obsoleted xrOS 1.0.99
  ios_obsoleted_16();
#ifdef XROS2
  // ios-error@-2 {{is unavailable: obsoleted in}}
#endif
  ios_ext_obsoleted_16();
}
