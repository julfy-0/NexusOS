#include "intel_gpu.h"

const char *intel_gpu_device_name(uint16_t id) {
    switch (id) {
        case 0x0102: return "2nd Gen Core Processor Graphics";
        case 0x0152: return "3rd Gen Core Processor Graphics";
        case 0x0162: return "3rd Gen Core Processor Graphics";
        case 0x0412: return "4th Gen Core Processor Graphics 4600";
        case 0x0416: return "4th Gen Core Processor Graphics 4400";
        case 0x0D22: return "4th Gen Core Processor Graphics 5000";
        case 0x0A16: return "HD Graphics Family";
        case 0x1616: return "HD Graphics 5500";
        case 0x1912: return "HD Graphics 530";
        case 0x191B: return "HD Graphics 530";
        case 0x5912: return "HD Graphics 630";
        case 0x5916: return "HD Graphics 620";
        case 0x3E92: return "UHD Graphics 630";
        case 0x3EA0: return "UHD Graphics 630";
        case 0x9BC4: return "UHD Graphics";
        case 0x9A49: return "Iris Xe Graphics";
        case 0x46A6: return "Iris Xe Graphics";
        case 0x4680: return "Arc Graphics";
        case 0x56A0: return "Arc Graphics";
        case 0x56A5: return "Arc Graphics";
        default: return "Intel Graphics";
    }
}
