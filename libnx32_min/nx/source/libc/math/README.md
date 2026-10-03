# libm subset (from musl)

The math functions Core32 needs (tinyexpr, lock.cpp, EGL.cpp), built into libnx32_min so Core32 doesn't
link newlib, whose ARM32 build isn't position independent.

## Origin and license

Taken from **musl 1.2.6** (`src/math/`, `src/math/arm/`, `src/internal/libm.h`).
musl is MIT licensed, see `COPYRIGHT.musl` (musl's `COPYRIGHT` file, unchanged). Some files carry their own
notices, which are kept as they are:

- files marked `origin: FreeBSD` come from FreeBSD msun / Sun fdlibm (Copyright (C) 1993 Sun Microsystems,
  free to use, copy, modify and distribute as long as the notice is kept)
- `exp*`, `log*`, `pow*`, `*_data.*`: Copyright (c) 2018 Arm Limited, MIT

## Changes from musl 1.2.6

- `libm.h`: silences musl's known `-Wparentheses` / `-Wmaybe-uninitialized` warnings (libnx32_min builds with
  `-Werror`), marks the `fp_force_eval*` temporaries unused, removes the `__signgam` / `__lgamma*_r`
  declarations (they clash with newlib's `math.h` and lgamma isn't used), includes `musl_features.h` instead of
  `<endian.h>` and drops `fp_arch.h`.
- `exp_data.h`, `log_data.h`, `pow_data.h`: include `musl_features.h` instead of `<features.h>`.
- `sqrt.c`, `fabs.c`: musl's ARM versions (`src/math/arm/`); the fallback to the generic C code is an `#error`,
  since the Switch always has a VFP double precision unit.
- `musl_features.h`: new, stand-in for the parts of musl's internal `<features.h>` / `<endian.h>` used here.

All other files are unchanged.
