#pragma once

#define SWIFT_SELF_CONTAINED __attribute__((swift_attr("Escapable")))
#define SWIFT_VIEW __attribute__((swift_attr("~Escapable")))
