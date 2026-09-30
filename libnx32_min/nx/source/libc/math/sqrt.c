// From musl 1.2.6 src/math/arm/sqrt.c (MIT, see COPYRIGHT.musl). Modified for libnx32_min (see README.md in this folder).
#include <math.h>

#if (__ARM_PCS_VFP || (__VFP_FP__ && !__SOFTFP__)) && (__ARM_FP&8)

double sqrt(double x)
{
	__asm__ ("vsqrt.f64 %P0, %P1" : "=w"(x) : "w"(x));
	return x;
}

#else

#error "sqrt needs a VFP double precision unit (always there on Switch)"

#endif
