#include "amd_gpu.h"

const char *amd_gpu_device_name(uint16_t id) {
    switch (id) {
        case 0x67DF: return "Radeon RX 580";
        case 0x6FDF: return "Radeon RX 590";
        case 0x731F: return "Radeon RX 5700 XT";
        case 0x7310: return "Radeon RX 5700";
        case 0x73BF: return "Radeon RX 6800 XT";
        case 0x73FF: return "Radeon RX 6900 XT";
        case 0x73DF: return "Radeon RX 6800";
        case 0x7422: return "Radeon RX 5500";
        case 0x743F: return "Radeon RX 5500 XT";
        case 0x15D8: return "Radeon Vega 8 Graphics";
        case 0x1636: return "Radeon Vega 8 Graphics";
        case 0x1638: return "Radeon Vega Graphics";
        case 0x164C: return "Radeon Graphics";
        case 0x164E: return "Radeon Graphics";
        case 0x15BF: return "Radeon 780M";
        case 0x15D1: return "Radeon 780M";
        case 0x744C: return "Radeon RX 7900 XTX";
        case 0x747E: return "Radeon RX 7800 XT";
        default: return "AMD Radeon Graphics";
    }
}
