#ifndef _OTA_MACRO_H_
#define _OTA_MACRO_H_

#include <stddef.h>

// Modify the initial address and size of OTA as needed
// DFU_PAN_LOADER_START_ADDR and DFU_PAN_LOADER_SIZE are defined based on 
// the OTA dedicated partition space configured in ptab.json
// The end portion of this space is used to store OTA-related data structures
#include "ptab.h"
#define DFU_PAN_FLASH_UNINIT_32     0xFFFFFFFF

#ifndef DFU_PAN_LOADER_START_ADDR
    #define DFU_PAN_LOADER_START_ADDR 0xFFFFFFFF
#endif

#ifndef DFU_PAN_LOADER_SIZE
    #define DFU_PAN_LOADER_SIZE 0xFFFFFFFF
#endif

// OTA info storage addresses
// OTA_INFO_BASE_ADDR: Points to the end of OTA partition space (last 4KB)
// VERSION_INFO_BASE_ADDR: Located just before OTA info (previous 4KB)
#define OTA_INFO_BASE_ADDR (DFU_PAN_LOADER_START_ADDR + DFU_PAN_LOADER_SIZE - 0x1000)
#define VERSION_INFO_BASE_ADDR (OTA_INFO_BASE_ADDR - 0x1000)  // The next 4KB is used to store version information.

// Define the number of supported version files
#define MAX_VERSION_FILES 5

// Store version information
struct version_info {
    char name[32];             
    char zippath[256];          
    uint32_t needs_update;         
};

// Automatically calculate the size of version_info structure
#define VERSION_INFO_SIZE sizeof(struct version_info)

// Calculate offset of needs_update field using offsetof macro
#define NEEDS_UPDATE_OFFSET offsetof(struct version_info, needs_update)

#endif