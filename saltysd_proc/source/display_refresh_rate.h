bool SetDisplayRefreshRate(uint32_t new_refreshRate);
bool GetDisplayRefreshRate(uint32_t* out_refreshRate, bool internal);
uint8_t getDockedHighestRefreshRateAllowed();
void correctOledGamma(uint32_t refresh_rate);