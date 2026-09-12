#include "nvidia_gpu.h"

const char *nvidia_gpu_device_name(uint16_t id) {
    switch (id) {
        case 0x1B80: return "GeForce GTX 1080";
        case 0x1B81: return "GeForce GTX 1070";
        case 0x1B06: return "GeForce GTX 1080 Ti";
        case 0x1C82: return "GeForce GTX 1050 Ti";
        case 0x1C03: return "GeForce GTX 1060 6GB";
        case 0x1E04: return "GeForce RTX 2080";
        case 0x1E07: return "GeForce RTX 2080 Ti";
        case 0x2184: return "GeForce RTX 2060";
        case 0x1F02: return "GeForce RTX 2070 Super";
        case 0x2204: return "GeForce RTX 2080 Super";
        case 0x2484: return "GeForce RTX 3070";
        case 0x2487: return "GeForce RTX 3070 Ti";
        case 0x2206: return "GeForce RTX 3080";
        case 0x2208: return "GeForce RTX 3080 Ti";
        case 0x2684: return "GeForce RTX 4060";
        case 0x2803: return "GeForce RTX 4060 Laptop GPU";
        case 0x2782: return "GeForce RTX 4070";
        case 0x2704: return "GeForce RTX 4090";
        case 0x2D01: return "GeForce RTX 5090";
        case 0x2D02: return "GeForce RTX 5080";
        default: return "NVIDIA GeForce / RTX Graphics";
    }
}
