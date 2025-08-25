#include <switch.h>
#include <cstring>
#include "useful.h"
#include <math.h>

#define	NVDISP_GET_MODE2 0x803C021B
#define	NVDISP_SET_MODE2 0x403C021C
#define NVDISP_VALIDATE_MODE2 0xC03C021D
#define NVDISP_GET_MODE_DB2 0xEF20021E
#define DSI_CLOCK_HZ 234000000llu
#define NVDISP_GET_AVI_INFOFRAME 0x80600210
#define NVDISP_SET_AVI_INFOFRAME 0x40600211
#define NVDISP_GET_PANEL_DATA 0xC01C0226

uint8_t dockedHighestRefreshRate = 60;
uint8_t dockedLinkRate = 10;
bool isRetroSUPER = false;
bool isPossiblySpoofedRetro = false;
bool wasRetroSuperTurnedOff = false;
uint32_t last_vActive = 1080;

extern uint64_t dsiVirtAddr;
extern bool isDocked;
extern bool dontForce60InDocked;
extern bool matchLowestDocked;
extern bool isLite;
extern uint64_t clkVirtAddr;
extern struct NxFpsSharedBlock* nx_fps;
extern bool displaySync;
extern bool displaySyncutOfFocus60;
extern bool displaySyncDocked;
extern bool displaySyncDockedOutOfFocus60;
extern SharedMemory _sharedMemory;

constexpr uint8_t DockedModeRefreshRateAllowedValues[] = {40, 45, 50, 55, 60, 70, 72, 75, 80, 90, 95, 100, 110, 120};
bool DockedModeRefreshRateAllowed[sizeof(DockedModeRefreshRateAllowedValues)] = {0};
bool DockedModeRefreshRateAllowed720p[sizeof(DockedModeRefreshRateAllowedValues)] = {0};

struct dockedTimings {
    uint16_t hFrontPorch;
    uint8_t hSyncWidth;
    uint8_t hBackPorch;
    uint8_t vFrontPorch;
    uint8_t vSyncWidth;
    uint8_t vBackPorch;
    uint8_t VIC;
    uint32_t pixelClock_kHz;
};

constexpr dockedTimings dockedTimings1080p[] =  {{8, 32, 40, 7, 8, 6, 0, 88080},        //40Hz CVT-RBv2
                                                {8, 32, 40, 9, 8, 6, 0, 99270},        //45Hz CVT-RBv2
                                                {528, 44, 148, 4, 5, 36, 31, 148500},  //50Hz CEA-861
                                                {8, 32, 40, 15, 8, 6, 0, 121990},      //55Hz CVT-RBv2
                                                {88, 44, 148, 4, 5, 36, 16, 148500},   //60Hz CEA-861
                                                {8, 32, 40, 22, 8, 6, 0, 156240},      //70Hz CVT-RBv2
                                                {8, 32, 40, 23, 8, 6, 0, 160848},      //72Hz CVT-RBv2
                                                {8, 32, 40, 25, 8, 6, 0, 167850},      //75Hz CVT-RBv2
                                                {8, 32, 40, 28, 8, 6, 0, 179520},      //80Hz CVT-RBv2
                                                {8, 32, 40, 33, 8, 6, 0, 202860},      //90Hz CVT-RBv2
                                                {8, 32, 40, 36, 8, 6, 0, 214700},      //95Hz CVT-RBv2
                                                {528, 44, 148, 4, 5, 36, 64, 297000},  //100Hz CEA-861
                                                {8, 32, 40, 44, 8, 6, 0, 250360},      //110Hz CVT-RBv2
                                                {88, 44, 148, 4, 5, 36, 63, 297000}};  //120Hz CEA-861

static_assert((sizeof(dockedTimings1080p) / sizeof(dockedTimings1080p[0])) == sizeof(DockedModeRefreshRateAllowed));

struct handheldTimings {
    uint8_t hSyncWidth;
    uint16_t hFrontPorch;
    uint8_t hBackPorch;
    uint8_t vSyncWidth;
    uint16_t vFrontPorch;
    uint8_t vBackPorch;
    uint32_t pixelClock_kHz;
};

constexpr handheldTimings handheldTimingsRETRO[] = {{72, 136, 72, 1, 660, 9, 78000},
                                                    {72, 136, 72, 1, 443, 9, 77985},
                                                    {72, 136, 72, 1, 270, 9, 78000},
                                                    {72, 136, 72, 1, 128, 9, 77990},
                                                    {72, 136, 72, 1, 10, 9, 78000}};

struct MinMax {
    u8 min;
    u8 max;
};

struct MinMax HandheldModeRefreshRateAllowed = {40, 60};

static_assert((sizeof(handheldTimingsRETRO) / sizeof(handheldTimingsRETRO[0])) == (((60 - 40) / 5) + 1));

struct PLLD_BASE {
    unsigned int PLLD_DIVM: 8;
    unsigned int reserved_1: 3;
    unsigned int PLLD_DIVN: 8;
    unsigned int reserved_2: 1;
    unsigned int PLLD_DIVP: 3;
    unsigned int CSI_CLK_SRC: 1;
    unsigned int reserved_3: 1;
    unsigned int PLL_D: 1;
    unsigned int reserved_4: 1;
    unsigned int PLLD_LOCK: 1; //Read Only
    unsigned int reserved_5: 1;
    unsigned int PLLD_REF_DIS: 1;
    unsigned int PLLD_ENABLE: 1;
    unsigned int PLLD_BYPASS: 1;
};

struct PLLD_MISC {
    signed int PLLD_SDM_DIN: 16;
    unsigned int PLLD_EN_SDM: 1;
    unsigned int PLLD_LOCK_OVERRIDE: 1;
    unsigned int PLLD_EN_LCKDET: 1;
    unsigned int PLLD_FREQLOCK: 1; //Read Only
    unsigned int PLLD_IDDQ: 1; //X
    unsigned int PLLD_ENABLE_CLK: 1;
    unsigned int PLLD_KVCO: 1;
    unsigned int PLLD_KCP: 2;
    unsigned int PLLD_PTS: 2;
    unsigned int PLLD_LDPULSE_ADJ: 3;
    unsigned int reserved: 2;
};

struct nvdcMode2 {
    unsigned int unk0;
    unsigned int hActive;
    unsigned int vActive;
    unsigned int hSyncWidth;
    unsigned int vSyncWidth;
    unsigned int hFrontPorch;
    unsigned int vFrontPorch;
    unsigned int hBackPorch;
    unsigned int vBackPorch;
    unsigned int pclkKHz;
    unsigned int bitsPerPixel;
    unsigned int vmode;
    unsigned int sync;
    unsigned int unk1;
    unsigned int reserved;
};

struct nvdcModeDB2 {
   struct nvdcMode2 modes[201];
   unsigned int num_modes;
};

struct dpaux_read_0x100 {
    u32 cmd;
    u32 addr;
    u32 size;
    struct {
        unsigned char link_rate;
        unsigned int lane_count: 5;
        unsigned int unk1: 2;
        unsigned int isFramingEnhanced: 1;
        unsigned char downspread;
        unsigned char training_pattern;
        unsigned char lane_pattern[4];
        unsigned char unk2[8];
    } set;
};

void changeOledElvssSettings(const uint32_t* offsets, const uint32_t* value, uint32_t size, uint32_t start) {
    if (!dsiVirtAddr || !value || !size) return;

    volatile uint32_t* dsiVirtAddr_impl = (uint32_t*)dsiVirtAddr;

    //Source: https://github.com/CTCaer/hekate/blob/master/bdk/display/di.h
    #define DSI_VIDEO_MODE_CONTROL        0x4E
    #define DSI_WR_DATA                   0xA
    #define DSI_TRIGGER                   0x13
    #define DSI_TRIGGER_VIDEO             0


    #define MIPI_DSI_DCS_SHORT_WRITE_PARAM  0x15
    #define MIPI_DSI_DCS_LONG_WRITE         0x39
    #define MIPI_DCS_PRIV_SM_SET_REG_OFFSET 0xB0
    #define MIPI_DCS_PRIV_SM_SET_ELVSS      0xB1

    dsiVirtAddr_impl[DSI_VIDEO_MODE_CONTROL] = true;
    svcSleepThread(20000000);

    dsiVirtAddr_impl[DSI_WR_DATA] = MIPI_DSI_DCS_LONG_WRITE | (5 << 8);
    dsiVirtAddr_impl[DSI_WR_DATA] = 0x5A5A5AE2;
    dsiVirtAddr_impl[DSI_WR_DATA] = 0x5A;
    dsiVirtAddr_impl[DSI_TRIGGER] = DSI_TRIGGER_VIDEO;

    for (size_t i = start; i < size; i++) {
        dsiVirtAddr_impl[DSI_WR_DATA] = ((MIPI_DCS_PRIV_SM_SET_REG_OFFSET | ((offsets[i] % 0x100) << 8)) << 8) | MIPI_DSI_DCS_SHORT_WRITE_PARAM;
        dsiVirtAddr_impl[DSI_TRIGGER] = DSI_TRIGGER_VIDEO;

        dsiVirtAddr_impl[DSI_WR_DATA] = ((MIPI_DCS_PRIV_SM_SET_ELVSS | (value[i] << 8)) << 8) | MIPI_DSI_DCS_SHORT_WRITE_PARAM;
        dsiVirtAddr_impl[DSI_TRIGGER] = DSI_TRIGGER_VIDEO;
    }

    dsiVirtAddr_impl[DSI_WR_DATA] = MIPI_DSI_DCS_LONG_WRITE | (5 << 8);
    dsiVirtAddr_impl[DSI_WR_DATA] = 0xA55A5AE2;
    dsiVirtAddr_impl[DSI_WR_DATA] = 0xA5;
    dsiVirtAddr_impl[DSI_TRIGGER] = DSI_TRIGGER_VIDEO;

    dsiVirtAddr_impl[DSI_VIDEO_MODE_CONTROL] = false;
    svcSleepThread(20000000);
}

extern "C" __attribute__((noinline)) void correctOledGamma(uint32_t refresh_rate) {
    static uint32_t last_refresh_rate = 60;
    if (isDocked || refresh_rate < 45 || refresh_rate > 60) {
        last_refresh_rate = 60;
        return;
    }
    static int i = 0;
    if (i != 9) {
        i++;
        return;
    }
    i = 0;
    #define loop_amount 5
    
    uint32_t offsets[] = {0x1A, 0x24, 0x25, 0x3D};
    uint32_t values_set[4] = {2, 0, 0x83, 0};
    if (refresh_rate == 60) {
        if (last_refresh_rate == 60) return;
    }
    else if (refresh_rate == 45) {
        uint32_t values[4] = {4, 1, 0, 3};
        if (last_refresh_rate == 45) return;
        memcpy(values_set, values, 16);

    }
    else if (refresh_rate == 50) {
        if (last_refresh_rate == 50) return;
        uint32_t values[4] = {4, 1, 0, 2};
        memcpy(values_set, values, 16);
  
    }
    else if (refresh_rate == 55) {
        if (last_refresh_rate == 55) return;
        uint32_t values[4] = {3, 1, 0, 2};
        memcpy(values_set, values, 16);
    }
    else return;
    for (size_t i = 0; i < loop_amount; i++) {
        changeOledElvssSettings(&offsets[0], &values_set[0], sizeof(offsets) / sizeof(offsets[0]), 0);
    }
    last_refresh_rate = refresh_rate;
}

void getDockedHighestRefreshRate(uint32_t fd_in) {
    uint8_t highestRefreshRate = 60;
    uint32_t fd = fd_in;
    if (!fd && R_FAILED(nvOpen(&fd, "/dev/nvdisp-disp1"))) {
        SaltySD_printf("SaltySD: Couldn't open /dev/nvdisp-disp1! Blocking to 60 Hz.\n");
        dockedHighestRefreshRate = 60;
        return;
    }
    struct nvdcModeDB2 DB2 = {0};
    Result nvrc = nvIoctl(fd, NVDISP_GET_MODE_DB2, &DB2);
    if (R_SUCCEEDED(nvrc)) {
        for (size_t i = 0; i < DB2.num_modes; i++) {
            if (DB2.modes[i].hActive < 1920 || DB2.modes[i].vActive < 1080) 
                continue;
            uint32_t v_total = DB2.modes[i].vActive + DB2.modes[i].vSyncWidth + DB2.modes[i].vFrontPorch + DB2.modes[i].vBackPorch;
            uint32_t h_total = DB2.modes[i].hActive + DB2.modes[i].hSyncWidth + DB2.modes[i].hFrontPorch + DB2.modes[i].hBackPorch;
            double refreshRate = round((double)(DB2.modes[i].pclkKHz * 1000) / (double)(v_total * h_total));
            if (highestRefreshRate < (uint8_t)refreshRate) highestRefreshRate = (uint8_t)refreshRate;
        }
    }
    else {
        SaltySD_printf("SaltySD: NVDISP_GET_MODE_DB2 for /dev/nvdisp-disp1 returned error 0x%x!\n", nvrc);
        dockedHighestRefreshRate = 60;
    }
    if (highestRefreshRate > DockedModeRefreshRateAllowedValues[sizeof(DockedModeRefreshRateAllowedValues) - 1]) 
        highestRefreshRate = DockedModeRefreshRateAllowedValues[sizeof(DockedModeRefreshRateAllowedValues) - 1];
    struct nvdcMode2 DISPLAY_B = {0};
    nvrc = nvIoctl(fd, NVDISP_GET_MODE2, &DISPLAY_B);
    if (R_FAILED(nvrc)) {
        SaltySD_printf("SaltySD: NVDISP_GET_MODE2 for /dev/nvdisp-disp1 returned error 0x%x!\n", nvrc);
    }
    struct dpaux_read_0x100 dpaux = {6, 0x100, 0x10};
    nvrc = nvIoctl(fd, NVDISP_GET_PANEL_DATA, &dpaux);
    if (R_SUCCEEDED(nvrc)) {
        dockedLinkRate = dpaux.set.link_rate;
        if (DISPLAY_B.hActive == 1920 && DISPLAY_B.vActive == 1080 && highestRefreshRate > 75 && dpaux.set.link_rate < 20) highestRefreshRate = 75;
    }
    else SaltySD_printf("SaltySD: NVDISP_GET_PANEL_DATA for /dev/nvdisp-disp1 returned error 0x%x!\n", nvrc);
    if (!fd_in) nvClose(fd);
    dockedHighestRefreshRate = highestRefreshRate;
}

extern "C" uint8_t getDockedHighestRefreshRateAllowed() {
    if (last_vActive == 1080) {
        for (size_t i = (sizeof(DockedModeRefreshRateAllowed) - 1); DockedModeRefreshRateAllowedValues[i] > 60; i--) {
            if (DockedModeRefreshRateAllowed[i] == true)
                return (DockedModeRefreshRateAllowedValues[i] > dockedHighestRefreshRate) ? dockedHighestRefreshRate : DockedModeRefreshRateAllowedValues[i];
        }
    }
    else if (last_vActive == 720) {
        for (size_t i = (sizeof(DockedModeRefreshRateAllowed720p) - 1); DockedModeRefreshRateAllowedValues[i] > 60; i--) {
            if (DockedModeRefreshRateAllowed720p[i] == true)
                return (DockedModeRefreshRateAllowedValues[i] > dockedHighestRefreshRate) ? dockedHighestRefreshRate : DockedModeRefreshRateAllowedValues[i];
        }        
    }
    return 60;
}

constexpr uint8_t getDockedRefreshRateIterator(uint32_t refreshRate) {
    for (size_t i = 0; i < sizeof(DockedModeRefreshRateAllowedValues); i++) {
        if (DockedModeRefreshRateAllowedValues[i] == refreshRate)
            return i;
    }
}

extern "C" void setAllowedDockedRefreshRatesIPC(uint32_t refreshRates, bool is720p) {
    struct {
        unsigned int Hz_40: 1;
        unsigned int Hz_45: 1;
        unsigned int Hz_50: 1;
        unsigned int Hz_55: 1;
        unsigned int Hz_60: 1;
        unsigned int Hz_70: 1;
        unsigned int Hz_72: 1;
        unsigned int Hz_75: 1;
        unsigned int Hz_80: 1;
        unsigned int Hz_90: 1;
        unsigned int Hz_95: 1;
        unsigned int Hz_100: 1;
        unsigned int Hz_110: 1;
        unsigned int Hz_120: 1;
        unsigned int reserved: 18;
    } DockedRefreshRates;

    static_assert(sizeof(DockedRefreshRates) == 4);

    memcpy(&DockedRefreshRates, &refreshRates, 4);
    if (!is720p) {
        DockedModeRefreshRateAllowed[getDockedRefreshRateIterator(40)] = DockedRefreshRates.Hz_40;
        DockedModeRefreshRateAllowed[getDockedRefreshRateIterator(45)] = DockedRefreshRates.Hz_45;
        DockedModeRefreshRateAllowed[getDockedRefreshRateIterator(50)] = DockedRefreshRates.Hz_50;
        DockedModeRefreshRateAllowed[getDockedRefreshRateIterator(55)] = DockedRefreshRates.Hz_55;
        DockedModeRefreshRateAllowed[getDockedRefreshRateIterator(60)] = true;
        DockedModeRefreshRateAllowed[getDockedRefreshRateIterator(70)] = DockedRefreshRates.Hz_70;
        DockedModeRefreshRateAllowed[getDockedRefreshRateIterator(72)] = DockedRefreshRates.Hz_72;
        DockedModeRefreshRateAllowed[getDockedRefreshRateIterator(75)] = DockedRefreshRates.Hz_75;
        DockedModeRefreshRateAllowed[getDockedRefreshRateIterator(80)] = DockedRefreshRates.Hz_80;
        DockedModeRefreshRateAllowed[getDockedRefreshRateIterator(90)] = DockedRefreshRates.Hz_90;
        DockedModeRefreshRateAllowed[getDockedRefreshRateIterator(95)] = DockedRefreshRates.Hz_95;
        DockedModeRefreshRateAllowed[getDockedRefreshRateIterator(100)] = DockedRefreshRates.Hz_100;
        DockedModeRefreshRateAllowed[getDockedRefreshRateIterator(110)] = DockedRefreshRates.Hz_110;
        DockedModeRefreshRateAllowed[getDockedRefreshRateIterator(120)] = DockedRefreshRates.Hz_120;
    }
    else {
        DockedModeRefreshRateAllowed720p[getDockedRefreshRateIterator(40)] = DockedRefreshRates.Hz_40;
        DockedModeRefreshRateAllowed720p[getDockedRefreshRateIterator(45)] = DockedRefreshRates.Hz_45;
        DockedModeRefreshRateAllowed720p[getDockedRefreshRateIterator(50)] = DockedRefreshRates.Hz_50;
        DockedModeRefreshRateAllowed720p[getDockedRefreshRateIterator(55)] = DockedRefreshRates.Hz_55;
        DockedModeRefreshRateAllowed720p[getDockedRefreshRateIterator(60)] = true;
        DockedModeRefreshRateAllowed720p[getDockedRefreshRateIterator(70)] = DockedRefreshRates.Hz_70;
        DockedModeRefreshRateAllowed720p[getDockedRefreshRateIterator(72)] = DockedRefreshRates.Hz_72;
        DockedModeRefreshRateAllowed720p[getDockedRefreshRateIterator(75)] = DockedRefreshRates.Hz_75;
        DockedModeRefreshRateAllowed720p[getDockedRefreshRateIterator(80)] = DockedRefreshRates.Hz_80;
        DockedModeRefreshRateAllowed720p[getDockedRefreshRateIterator(90)] = DockedRefreshRates.Hz_90;
        DockedModeRefreshRateAllowed720p[getDockedRefreshRateIterator(95)] = DockedRefreshRates.Hz_95;
        DockedModeRefreshRateAllowed720p[getDockedRefreshRateIterator(100)] = DockedRefreshRates.Hz_100;
        DockedModeRefreshRateAllowed720p[getDockedRefreshRateIterator(110)] = DockedRefreshRates.Hz_110;
        DockedModeRefreshRateAllowed720p[getDockedRefreshRateIterator(120)] = DockedRefreshRates.Hz_120;
    }
}

constexpr void setDefaultDockedSettings() {
    memset(DockedModeRefreshRateAllowed, 0, sizeof(DockedModeRefreshRateAllowed));
    DockedModeRefreshRateAllowed[getDockedRefreshRateIterator(50)] = true;
    DockedModeRefreshRateAllowed[getDockedRefreshRateIterator(60)] = true;
    memset(DockedModeRefreshRateAllowed720p, 0, sizeof(DockedModeRefreshRateAllowed720p));
    DockedModeRefreshRateAllowed720p[getDockedRefreshRateIterator(50)] = true;
    DockedModeRefreshRateAllowed720p[getDockedRefreshRateIterator(60)] = true;
    dontForce60InDocked = false;
    matchLowestDocked = false;
    displaySyncDockedOutOfFocus60 = false;
}

void LoadDockedModeAllowedSave() {
    SetSysEdid edid = {0};
    setDefaultDockedSettings();
    if (isLite)
        return;
    if (R_FAILED(setsysGetEdid(&edid))) {
        SaltySD_printf("SaltySD: Couldn't retrieve display EDID! Locking allowed refresh rates in docked mode to 50 and 60 Hz.\n");
        return;
    }
    char path[128] = "";
    int crc32 = crc32Calculate(&edid, sizeof(edid));
    snprintf(path, sizeof(path), "sdmc:/SaltySD/plugins/FPSLocker/ExtDisplays/%08X.dat", crc32);
    if (file_or_directory_exists(path) == false) {
        FILE* file = fopen(path, "wb");
        if (file) {
            fwrite(&edid, sizeof(edid), 1, file);
            fclose(file);
        }
        else SaltySD_printf("SaltySD: Couldn't dump EDID to sdcard!\n", &path[31]);
    }
    snprintf(path, sizeof(path), "sdmc:/SaltySD/plugins/FPSLocker/ExtDisplays/%08X.ini", crc32);
    if (file_or_directory_exists(path) == true) {
        FILE* file = fopen(path, "r");
        if (!file) {
            SaltySD_printf("SaltySD: %s opening failed (file already opened?). Locking allowed refresh rates in docked mode to 50 and 60 Hz.\n", &path[31]);
            return;
        }
        fseek(file, 0, 2);
        size_t size = ftell(file);
        fseek(file, 0, 0);
        char* temp_string = (char*)malloc(size);
        if (!temp_string) {
            SaltySD_printf("SaltySD: Allocation failure! Memory leak. Get ready for crash.\n");
        }
        fread(temp_string, size, 1, file);
        fclose(file);
        remove_spaces(temp_string, temp_string);
        if (memcmp(temp_string, "[Common]", 8)) {
            SaltySD_printf("SaltySD: %s doesn't start with \"[Common]\"! Using default settings!\n", &path[31]);
            free(temp_string);
            return;
        }
        char* substring = strstr(temp_string, "refreshRateAllowed={");
        if (substring == NULL) {
            SaltySD_printf("SaltySD: %s doesn't have \"refreshRateAllowed\"! Using default settings!\n", &path[31]);
            free(temp_string);
            return;
        }
        char* rr_start = &substring[strlen("refreshRateAllowed={")];
        substring = strstr(rr_start, "}");
        if (substring == NULL) {
            SaltySD_printf("SaltySD: %s \"refreshRateAllowed\" is malformed! Using default settings!\n", &path[31]);
            free(temp_string);
            return;
        }
        size_t amount = 1;
        for (size_t i = 0; i < ((size_t)substring - (size_t)rr_start); i++) {
            if (rr_start[i] == ',') amount++;
        }
        for (size_t i = 0; i < amount; i++) {
            long value = strtol(rr_start, &rr_start, 10);
            if ((i+1 == amount) && (rr_start[0] != '}')) return;
            if ((i+1 < amount) && (rr_start[0] != ',')) return;
            rr_start = &rr_start[1];
            if (value < 40 || value > 240) continue;
            for (size_t i = 0; i < sizeof(DockedModeRefreshRateAllowed); i++) {
                if (value == DockedModeRefreshRateAllowedValues[i]) {
                    DockedModeRefreshRateAllowed[i] = true;
                    break;
                }
            }
        }
        substring = strstr(temp_string, "allowPatchesToForce60InDocked=");
        if (substring != NULL) {
            substring = &substring[strlen("allowPatchesToForce60InDocked=")];
            dontForce60InDocked = (bool)!strncasecmp(substring, "False", 5);
        }
        else SaltySD_printf("SaltySD: %s doesn't have \"allowPatchesToForce60InDocked\"! Setting to true!\n", &path[31]);
        substring = strstr(temp_string, "matchLowestRefreshRate=");
        if (substring != NULL) {
            substring = &substring[strlen("matchLowestRefreshRate=")];
            matchLowestDocked = (bool)!strncasecmp(substring, "True", 4);
        }
        else SaltySD_printf("SaltySD: %s doesn't have \"matchLowestRefreshRate\"! Setting to false!\n", &path[31]);
        substring = strstr(temp_string, "bringDefaultRefreshRateWhenOutOfFocus=");
        if (substring != NULL) {
            substring = &substring[strlen("bringDefaultRefreshRateWhenOutOfFocus=")];
            displaySyncDockedOutOfFocus60 = (bool)!strncasecmp(substring, "True", 4);
        }
        else SaltySD_printf("SaltySD: %s doesn't have \"bringDefaultRefreshRateWhenOutOfFocus=\"! Setting to false!\n", &path[31]);
        substring = strstr(temp_string, "refreshRateAllowed720p={");
        if (substring == NULL) {
            SaltySD_printf("SaltySD: %s doesn't have \"refreshRateAllowed720p\"! Using default settings!\n", &path[31]);
            free(temp_string);
            return;
        }
        rr_start = &substring[strlen("refreshRateAllowed720p={")];
        substring = strstr(rr_start, "}");
        if (substring == NULL) {
            SaltySD_printf("SaltySD: %s \"refreshRateAllowed720p\" is malformed! Using default settings!\n", &path[31]);
            free(temp_string);
            return;
        }
        amount = 1;
        for (size_t i = 0; i < ((size_t)substring - (size_t)rr_start); i++) {
            if (rr_start[i] == ',') amount++;
        }
        for (size_t i = 0; i < amount; i++) {
            long value = strtol(rr_start, &rr_start, 10);
            if ((i+1 == amount) && (rr_start[0] != '}')) return;
            if ((i+1 < amount) && (rr_start[0] != ',')) return;
            rr_start = &rr_start[1];
            if (value < 40 || value > 240) continue;
            for (size_t i = 0; i < sizeof(DockedModeRefreshRateAllowed720p); i++) {
                if (value == DockedModeRefreshRateAllowedValues[i]) {
                    DockedModeRefreshRateAllowed720p[i] = true;
                    break;
                }
            }
        }
        free(temp_string);
    }
    else {
        SaltySD_printf("SaltySD: File \"%s\" not found! Locking allowed refresh rates in docked mode to 50 and 60 Hz.\n", path);
    }
}

bool canChangeRefreshRateDocked = false;

struct dpaux_read {
    u32 cmd;
    u32 addr;
    u32 size;
    struct {
        unsigned int rev_minor : 4;
        unsigned int rev_major : 4;
        unsigned char link_rate;
        unsigned int lane_count: 5;
        unsigned int unk1: 2;
        unsigned int isFramingEnhanced: 1;
        unsigned char unk2[13];
    } DPCD;
};

bool setPLLDHandheldRefreshRate(uint32_t new_refreshRate) {
    if (!clkVirtAddr) return false;

    uint32_t fd = 0;
    if (R_FAILED(nvOpen(&fd, "/dev/nvdisp-disp0"))) {
        return false;
    }
    struct dpaux_read dpaux = {6, 0, 0x10};
    Result rc = nvIoctl(fd, NVDISP_GET_PANEL_DATA, &dpaux);
    nvClose(fd);
    if (rc != 0x75c) return false;

    struct PLLD_BASE base = {0};
    struct PLLD_MISC misc = {0};
    memcpy(&base, (void*)(clkVirtAddr + 0xD0), 4);
    memcpy(&misc, (void*)(clkVirtAddr + 0xDC), 4);
    uint32_t value = ((base.PLLD_DIVN / base.PLLD_DIVM) * 10) / 4;
    if (value == 0 || value == 80) return false;
    //We are in handheld mode
    
    if (new_refreshRate > HandheldModeRefreshRateAllowed.max) {
        new_refreshRate = HandheldModeRefreshRateAllowed.max;
    }
    else if (new_refreshRate < HandheldModeRefreshRateAllowed.min) {
        bool skip = false;
        for (size_t i = 2; i <= 4; i++) {
            if (new_refreshRate * i == 60) {
                skip = true;
                new_refreshRate = 60;
                break;
            }
        }
        if (!skip) for (size_t i = 2; i <= 4; i++) {
            if (((new_refreshRate * i) >= HandheldModeRefreshRateAllowed.min) && ((new_refreshRate * i) <= HandheldModeRefreshRateAllowed.max)) {
                skip = true;
                new_refreshRate *= i;
                break;
            }
        }
        if (!skip) new_refreshRate = 60;
    }
    uint32_t pixelClock = (9375 * ((4096 * ((2 * base.PLLD_DIVN) + 1)) + misc.PLLD_SDM_DIN)) / (8 * base.PLLD_DIVM);
    uint16_t refreshRateNow = pixelClock / (DSI_CLOCK_HZ / 60);

    if (refreshRateNow == new_refreshRate) {
        if (nx_fps) nx_fps->currentRefreshRate = new_refreshRate;
        return true;
    }

    uint8_t base_refreshRate = new_refreshRate - (new_refreshRate % 5);

    base.PLLD_DIVN = (4 * base_refreshRate) / 10;
    base.PLLD_DIVM = 1;

    uint64_t expected_pixel_clock = (DSI_CLOCK_HZ * new_refreshRate) / 60;

    misc.PLLD_SDM_DIN = ((8 * base.PLLD_DIVM * expected_pixel_clock) / 9375) - (4096 * ((2 * base.PLLD_DIVN)+1));

    memcpy((void*)(clkVirtAddr + 0xD0), &base, 4);
    memcpy((void*)(clkVirtAddr + 0xDC), &misc, 4);
    return true;
}

bool setNvDispDockedRefreshRate(uint32_t new_refreshRate) {
    static uint8_t last_vActive_set = 0;
    if (isLite || !canChangeRefreshRateDocked)
        return false;
    uint32_t fd = 0;
    if (R_FAILED(nvOpen(&fd, "/dev/nvdisp-disp1"))) {
        return false;
    }
    struct nvdcMode2 DISPLAY_B = {0};
    Result nvrc = nvIoctl(fd, NVDISP_GET_MODE2, &DISPLAY_B);
    if (R_FAILED(nvrc)) {
        SaltySD_printf("SaltySD: NVDISP_GET_MODE2 failed! rc: 0x%x\n", nvrc);
        nvClose(fd);
        return false;
    }
    if (!DISPLAY_B.pclkKHz) {
        nvClose(fd);
        return false;
    }
    if (((DISPLAY_B.vActive == 480 && DISPLAY_B.hActive == 720) || (DISPLAY_B.vActive == 720 && DISPLAY_B.hActive == 1280) || (DISPLAY_B.vActive == 1080 && DISPLAY_B.hActive == 1920)) == false) {
        nvClose(fd);
        return false;
    }
    if ((file_or_directory_exists("sdmc:/SaltySD/test.flag") == false) && DISPLAY_B.vActive != last_vActive_set) {
        last_vActive_set = DISPLAY_B.vActive;
        if (DISPLAY_B.vActive != 720 && DISPLAY_B.vActive != 1080) {
            memset(DockedModeRefreshRateAllowed, 0, sizeof(DockedModeRefreshRateAllowed));
            DockedModeRefreshRateAllowed[getDockedRefreshRateIterator(60)] = true;
        }
    }
    uint32_t h_total = DISPLAY_B.hActive + DISPLAY_B.hFrontPorch + DISPLAY_B.hSyncWidth + DISPLAY_B.hBackPorch;
    uint32_t v_total = DISPLAY_B.vActive + DISPLAY_B.vFrontPorch + DISPLAY_B.vSyncWidth + DISPLAY_B.vBackPorch;
    uint32_t refreshRateNow = ((DISPLAY_B.pclkKHz) * 1000 + 999) / (h_total * v_total);
    int8_t itr = -1;
    if ((new_refreshRate <= 60) && ((60 % new_refreshRate) == 0)) {
        itr = getDockedRefreshRateIterator(60);
    }
    if (itr == -1) for (size_t i = 0; i < sizeof(DockedModeRefreshRateAllowedValues); i++) {
        if (DISPLAY_B.vActive == 720 && DockedModeRefreshRateAllowed720p[i] != true)
            continue;
        else if (DISPLAY_B.vActive == 1080 && DockedModeRefreshRateAllowed[i] != true)
            continue;
        uint8_t val = DockedModeRefreshRateAllowedValues[i];
        if ((val % new_refreshRate) == 0) {
            itr = i;
            break;
        }
    }
    if (itr == -1) {
        if (!matchLowestDocked)
            itr = getDockedRefreshRateIterator(60);
        else {
            if (DISPLAY_B.vActive == 1080) for (size_t i = 0; i < sizeof(DockedModeRefreshRateAllowed); i++) {
                if ((DockedModeRefreshRateAllowed[i] == true) && (new_refreshRate < DockedModeRefreshRateAllowedValues[i])) {
                    itr = i;
                    break;
                }
            }
            else if (DISPLAY_B.vActive == 720) for (size_t i = 0; i < sizeof(DockedModeRefreshRateAllowed720p); i++) {
                if ((DockedModeRefreshRateAllowed720p[i] == true) && (new_refreshRate < DockedModeRefreshRateAllowedValues[i])) {
                    itr = i;
                    break;
                }
            }
        }
    }
    if (itr == -1) itr = getDockedRefreshRateIterator(60);
    bool increase = refreshRateNow < DockedModeRefreshRateAllowedValues[itr];
    while(itr >= 0 && itr < (int8_t)sizeof(DockedModeRefreshRateAllowedValues) && ((DISPLAY_B.vActive != 720) ? (DockedModeRefreshRateAllowed[itr] != true) : (DockedModeRefreshRateAllowed720p[itr] != true))) {
        if (!displaySyncDocked) {
            if (increase) itr++;
            else itr--;
        }
        else itr++;
    }
    if (refreshRateNow == DockedModeRefreshRateAllowedValues[itr]) {
        if (nx_fps) nx_fps->currentRefreshRate = DockedModeRefreshRateAllowedValues[itr];
        nvClose(fd);
        return true;
    }
    
    if (itr >= 0 && itr < (int8_t)sizeof(DockedModeRefreshRateAllowed)) {
        if (DISPLAY_B.vActive == 720) {
            uint32_t clock = ((h_total * v_total) * DockedModeRefreshRateAllowedValues[itr]) / 1000;
            DISPLAY_B.pclkKHz = clock;
        }
        else {
            DISPLAY_B.hFrontPorch = dockedTimings1080p[itr].hFrontPorch;
            DISPLAY_B.hSyncWidth = dockedTimings1080p[itr].hSyncWidth;
            DISPLAY_B.hBackPorch = dockedTimings1080p[itr].hBackPorch;
            DISPLAY_B.vFrontPorch = dockedTimings1080p[itr].vFrontPorch;
            DISPLAY_B.vSyncWidth = dockedTimings1080p[itr].vSyncWidth;
            DISPLAY_B.vBackPorch = dockedTimings1080p[itr].vBackPorch;
            DISPLAY_B.pclkKHz = dockedTimings1080p[itr].pixelClock_kHz;
            DISPLAY_B.vmode = (DockedModeRefreshRateAllowedValues[itr] >= 100 ? 0x400000 : 0x200000);
            DISPLAY_B.unk1 = (DockedModeRefreshRateAllowedValues[itr] >= 100 ? 0x80 : 0);
            DISPLAY_B.sync = 3;
            DISPLAY_B.bitsPerPixel = 24;
        }
        nvrc = nvIoctl(fd, NVDISP_VALIDATE_MODE2, &DISPLAY_B);
        if (R_SUCCEEDED(nvrc)) {
            nvrc = nvIoctl(fd, NVDISP_SET_MODE2, &DISPLAY_B);
            if (R_FAILED(nvrc)) SaltySD_printf("SaltySD: NVDISP_SET_MODE2 failed! rc: 0x%x\n", nvrc);
            else if (nx_fps) nx_fps->currentRefreshRate = DockedModeRefreshRateAllowedValues[itr];
        }
        else SaltySD_printf("SaltySD: NVDISP_VALIDATE_MODE2 failed! rc: 0x%x, pclkKHz: %d, Hz: %d\n", nvrc, clock, DockedModeRefreshRateAllowedValues[itr]);
    }
    nvClose(fd);
    return true;
}

bool setNvDispHandheldRefreshRate(uint32_t new_refreshRate) {
    if (!isRetroSUPER)
        return false;
    if (!displaySync) {
        wasRetroSuperTurnedOff = false;
    }
    else if (wasRetroSuperTurnedOff) {
        svcSleepThread(2000000000);
        wasRetroSuperTurnedOff = false;
    }
    svcSleepThread(1000000000);
    uint32_t fd = 0;
    if (R_FAILED(nvOpen(&fd, "/dev/nvdisp-disp0"))) {
        SaltySD_printf("SaltySD: Couldn't open nvdisp-disp0 for Retro Remake!\n");
        return false;
    }
    struct nvdcMode2 DISPLAY_B = {0};
    Result nvrc = nvIoctl(fd, NVDISP_GET_MODE2, &DISPLAY_B);
    if (R_FAILED(nvrc)) {
        SaltySD_printf("SaltySD: NVDISP_GET_MODE2 failed! rc: 0x%x\n", nvrc);
        nvClose(fd);
        return false;
    }
    if (!DISPLAY_B.pclkKHz) {
        nvClose(fd);
        return false;
    }
    if ((DISPLAY_B.vActive == 1280 && DISPLAY_B.hActive == 720) == false) {
        nvClose(fd);
        return false;
    }
    //720 + 72 + 136 + 72
    uint32_t h_total = DISPLAY_B.hActive + DISPLAY_B.hFrontPorch + DISPLAY_B.hSyncWidth + DISPLAY_B.hBackPorch;
    //1280 + 1 + 10 + 9
    uint32_t v_total = DISPLAY_B.vActive + DISPLAY_B.vFrontPorch + DISPLAY_B.vSyncWidth + DISPLAY_B.vBackPorch;
    uint32_t refreshRateNow = ((DISPLAY_B.pclkKHz) * 1000 + 999) / (h_total * v_total);

    if (new_refreshRate > HandheldModeRefreshRateAllowed.max) {
        new_refreshRate = HandheldModeRefreshRateAllowed.max;
    }
    else if (new_refreshRate < HandheldModeRefreshRateAllowed.min) {
        bool skip = false;
        for (size_t i = 2; i <= 4; i++) {
            if (new_refreshRate * i == 60) {
                skip = true;
                new_refreshRate = 60;
                break;
            }
        }
        if (!skip) for (size_t i = 2; i <= (sizeof(handheldTimingsRETRO) / sizeof(handheldTimingsRETRO[0])); i++) {
            if (((new_refreshRate * i) >= HandheldModeRefreshRateAllowed.min) && ((new_refreshRate * i) <= HandheldModeRefreshRateAllowed.max)) {
                skip = true;
                new_refreshRate *= i;
                break;
            }
        }
        if (!skip) new_refreshRate = 60;
    }
    if (new_refreshRate == refreshRateNow) {
        nvClose(fd);
        return true;
    }

    uint32_t itr = (new_refreshRate - 40) / 5;

    DISPLAY_B.hFrontPorch = handheldTimingsRETRO[itr].hFrontPorch;
    DISPLAY_B.hSyncWidth = handheldTimingsRETRO[itr].hSyncWidth;
    DISPLAY_B.hBackPorch = handheldTimingsRETRO[itr].hBackPorch;
    DISPLAY_B.vFrontPorch = handheldTimingsRETRO[itr].vFrontPorch;
    DISPLAY_B.vSyncWidth = handheldTimingsRETRO[itr].vSyncWidth;
    DISPLAY_B.vBackPorch = handheldTimingsRETRO[itr].vBackPorch;
    DISPLAY_B.pclkKHz = handheldTimingsRETRO[itr].pixelClock_kHz;

    nvrc = nvIoctl(fd, NVDISP_VALIDATE_MODE2, &DISPLAY_B);
    if (R_SUCCEEDED(nvrc)) {
        for (size_t i = 0; i < 5; i++) {
            nvrc = nvIoctl(fd, NVDISP_SET_MODE2, &DISPLAY_B);
        }
        if (R_FAILED(nvrc)) SaltySD_printf("SaltySD: NVDISP_SET_MODE2 failed! rc: 0x%x\n", nvrc);
        else if (nx_fps) nx_fps->currentRefreshRate = new_refreshRate;
    }
    else SaltySD_printf("SaltySD: NVDISP_VALIDATE_MODE2 failed! rc: 0x%x, pclkKHz: %d, Hz: %d\n", nvrc, DISPLAY_B.pclkKHz, new_refreshRate);
    nvClose(fd);
    return true;
}

extern "C" bool SetDisplayRefreshRate(uint32_t new_refreshRate) {
    if (!new_refreshRate)
        return false;

    u32 fd = 0;
    
    if (isLite && isPossiblySpoofedRetro) {
        if (file_or_directory_exists("sdmc:/SaltySD/flags/retro.flag") == true)
            isRetroSUPER = true;
        else isRetroSUPER = false;
    }
    
    if (isRetroSUPER && !isDocked) {
        if (setNvDispHandheldRefreshRate(new_refreshRate) == false)
            return false;
    }
    else if ((!isRetroSUPER && isLite) || R_FAILED(nvOpen(&fd, "/dev/nvdisp-disp1"))) {
        if (setPLLDHandheldRefreshRate(new_refreshRate) == false) 
            return false;
    }
    else {
        struct dpaux_read dpaux = {6, 0, 0x10};
        Result rc = nvIoctl(fd, NVDISP_GET_PANEL_DATA, &dpaux);
        nvClose(fd);
        bool return_immediately = false;
        if (R_FAILED(rc)) {
            if (!isRetroSUPER) return_immediately = !setPLLDHandheldRefreshRate(new_refreshRate);
            else return_immediately = !setNvDispHandheldRefreshRate(new_refreshRate); //Used only for Retro Remake displays because they are very picky about pixel clock
        }
        else return_immediately = !setNvDispDockedRefreshRate(new_refreshRate);
        if (return_immediately) return false;
    }
    if (nx_fps) nx_fps->currentRefreshRate = new_refreshRate;
    return true;
}

extern "C" bool GetDisplayRefreshRate(uint32_t* out_refreshRate, bool internal) {
    if (!clkVirtAddr)
        return false;
    uint32_t value = 60;
    uintptr_t sh_addr = (uintptr_t)shmemGetAddr(&_sharedMemory);
    if (!internal) {
        // We are using this trick because using nvOpen severes connection 
        // with whatever is actually connected to this sysmodule
        if (sh_addr) {
            *out_refreshRate = *(uint8_t*)(sh_addr + 1);
            return true;
        }
        else return false;
    }
    if (isRetroSUPER && !isDocked) {
        u32 fd = 0;
        struct PLLD_BASE temp = {0};
        struct PLLD_MISC misc = {0};
        memcpy(&temp, (void*)(clkVirtAddr + 0xD0), 4);
        memcpy(&misc, (void*)(clkVirtAddr + 0xDC), 4);
        value = ((temp.PLLD_DIVN / temp.PLLD_DIVM) * 10) / 4;
        if (value != 0 && value != 80) {
            if (R_SUCCEEDED(nvOpen(&fd, "/dev/nvdisp-disp0"))) {
                struct nvdcMode2 DISPLAY_B = {0};
                if (R_SUCCEEDED(nvIoctl(fd, NVDISP_GET_MODE2, &DISPLAY_B))) {
                    uint32_t h_total = DISPLAY_B.hActive + DISPLAY_B.hFrontPorch + DISPLAY_B.hSyncWidth + DISPLAY_B.hBackPorch;
                    uint32_t v_total = DISPLAY_B.vActive + DISPLAY_B.vFrontPorch + DISPLAY_B.vSyncWidth + DISPLAY_B.vBackPorch;
                    uint32_t pixelClock = DISPLAY_B.pclkKHz * 1000 + 999;
                    value = pixelClock / (h_total * v_total);                
                }
                nvClose(fd);
            }
            else return false;
        }
        else wasRetroSuperTurnedOff = true;
    }
    else if ((!isPossiblySpoofedRetro) || (isPossiblySpoofedRetro && !isRetroSUPER)) {
        struct PLLD_BASE temp = {0};
        struct PLLD_MISC misc = {0};
        memcpy(&temp, (void*)(clkVirtAddr + 0xD0), 4);
        memcpy(&misc, (void*)(clkVirtAddr + 0xDC), 4);
        value = ((temp.PLLD_DIVN / temp.PLLD_DIVM) * 10) / 4;
        if (value == 0 || value == 80) { //We are in docked mode
            if (isLite)
                return false;
            isDocked = true;
            //We must add delay for changing refresh rate when it was just put into dock to avoid doing calculation on default values instead of adjusted ones
            //From my tests 1 second is enough
            if (!canChangeRefreshRateDocked) {
                u32 fd = 0;
                if (R_SUCCEEDED(nvOpen(&fd, "/dev/nvdisp-disp1"))) {
                    struct dpaux_read_0x100 dpaux = {6, 0x100, 0x10};
                    Result nvrc = nvIoctl(fd, NVDISP_GET_PANEL_DATA, &dpaux);
                    nvClose(fd);
                    if (R_SUCCEEDED(nvrc)) {
                        LoadDockedModeAllowedSave();
                        getDockedHighestRefreshRate(0);
                        canChangeRefreshRateDocked = true;
                    }
                    else {
                        svcSleepThread(1'000'000'000);
                        return false;
                    }
                }
                else return false;
            }
            uint32_t fd = 0;
            if (R_SUCCEEDED(nvOpen(&fd, "/dev/nvdisp-disp1"))) {
                struct nvdcMode2 DISPLAY_B = {0};
                if (R_SUCCEEDED(nvIoctl(fd, NVDISP_GET_MODE2, &DISPLAY_B))) {
                    if (!DISPLAY_B.pclkKHz) {
                        nvClose(fd);
                        return false;
                    }
                    if (last_vActive != DISPLAY_B.vActive) {
                        last_vActive = DISPLAY_B.vActive;
                        getDockedHighestRefreshRate(fd);
                    }
                    uint32_t h_total = DISPLAY_B.hActive + DISPLAY_B.hFrontPorch + DISPLAY_B.hSyncWidth + DISPLAY_B.hBackPorch;
                    uint32_t v_total = DISPLAY_B.vActive + DISPLAY_B.vFrontPorch + DISPLAY_B.vSyncWidth + DISPLAY_B.vBackPorch;
                    uint32_t pixelClock = DISPLAY_B.pclkKHz * 1000 + 999;
                    value = pixelClock / (h_total * v_total);
                }
                else value = 60;
                nvClose(fd);
            }
            else value = 60;
        }
        else if (!isRetroSUPER) {
            isDocked = false;
            canChangeRefreshRateDocked = false;
            //We are in handheld mode
            /*
                Official formula:
                Fvco = Fref / DIVM * (DIVN + 0.5 + (SDM_DIN / 8192))
                Fref = CNTFRQ_EL0 / 2
                Defaults: DIVM = 1, DIVN = 24, SDM_DIN = -1024

                My math formula allows avoiding decimals whenever possible
            */
            uint32_t pixelClock = (9375 * ((4096 * ((2 * temp.PLLD_DIVN) + 1)) + misc.PLLD_SDM_DIN)) / (8 * temp.PLLD_DIVM);
            value = pixelClock / (DSI_CLOCK_HZ / 60);
        }
        else return false;
    }
    *out_refreshRate = value;
    if (sh_addr) 
        *(uint8_t*)(sh_addr + 1) = (uint8_t)value;
    if (nx_fps)
        nx_fps -> currentRefreshRate = (uint8_t)value;
    return true;
}