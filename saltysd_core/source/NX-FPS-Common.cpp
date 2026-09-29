#include "NX-FPS-Common.hpp"

namespace nn {
	Result SetUserInactivityDetectionTimeExtended(bool isTrue) {
		return SetUserInactivityDetectionTimeExtended_0(isTrue);
	}
}

namespace Utils {
	uint64_t _convertToTimeSpan(uint64_t tick) {
		#if defined(SWITCH) || defined(SWITCH32)
			return armTicksToNs(tick);
		#elif defined(OUNCE) || defined(OUNCE32)
			return tick << 5;
		#else
			return uint64_t((double)tick / ((double)(systemtickfrequency) / 1000000000.d));
		#endif
	}
}
