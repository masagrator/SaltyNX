// Stand-ins for musl's internal <features.h> and <endian.h>, just what the math sources here use.
// The hidden/weak_alias definitions are musl's (musl 1.2.6 src/include/features.h, MIT, see COPYRIGHT.musl).
#pragma once
#define hidden __attribute__((__visibility__("hidden")))
#define weak_alias(old, new) extern __typeof(old) new __attribute__((__weak__, __alias__(#old)))
#define __LITTLE_ENDIAN 1234
#define __BIG_ENDIAN 4321
#define __BYTE_ORDER __LITTLE_ENDIAN
