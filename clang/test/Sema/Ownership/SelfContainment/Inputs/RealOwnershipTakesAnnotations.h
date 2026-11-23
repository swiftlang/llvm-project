#pragma once

#define OWNERSHIP_TAKES(N) __attribute__((ownership_takes(release, N)))
