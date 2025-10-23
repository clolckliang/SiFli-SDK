#ifndef _BT_PAN_OTA_H_
#define _BT_PAN_OTA_H_

#include "bts2_app_inc.h"
#include <stdint.h>
#include "dfu_pan_macro.h"
#include "drv_flash.h"
extern char current_version[32];         

// Define the number of supported firmware files
#define MAX_FIRMWARE_FILES 3
#define MAX_VERSION_COUNT MAX_VERSION_FILES

// Base header field offsets and sizes
#define HEADER_CRC_OFFSET 0             // Offset of CRC field in base header
#define HEADER_CRC_SIZE 4               // Size of CRC field in base header
#define HEADER_MAGIC_OFFSET 4           // Offset of magic field in base header
#define HEADER_MAGIC_SIZE 4             // Size of magic field in base header
#define HEADER_VERSION_OFFSET 8         // Offset of version field in base header
#define HEADER_VERSION_SIZE 4           // Size of version field in base header
#define HEADER_BIN_NUM_OFFSET 12        // Offset of bin number field in base header
#define HEADER_BIN_NUM_SIZE 4           // Size of bin number field in base header

// Firmware package header constants
#define OTA_PACKAGE_MAGIC 0x5F4F5441    // "_OTA" in hex
#define BASE_HEADER_SIZE 16             // Size of the base package header
#define PER_FILE_HEADER_SIZE 76         // Size of each bin file header
#define FILE_NAME_SIZE 48               // Size of the file name field
#define FILE_NAME_FIELD_SIZE 48         // Size of the file name field
#define FILE_ID_OFFSET 48               // Offset of ID field in file header
#define FILE_ID_SIZE 2                  // Size of ID field in file header
#define FILE_GZIP_OFFSET 50             // Offset of GZIP field in file header
#define FILE_GZIP_SIZE 2                // Size of GZIP field in file header
#define FILE_COMPRESSED_LEN_OFFSET 52   // Offset of compressed length field
#define FILE_COMPRESSED_LEN_SIZE 4      // Size of compressed length field
#define FILE_ORIG_LEN_OFFSET 56         // Offset of original length field
#define FILE_ORIG_LEN_SIZE 4            // Size of original length field
#define FILE_ADDR_OFFSET 60             // Offset of address field
#define FILE_ADDR_SIZE 4                // Size of address field
#define FILE_CRC_OFFSET 64              // Offset of CRC field
#define FILE_CRC_SIZE 4                 // Size of CRC field
#define FILE_ORIG_CRC_OFFSET 68         // Offset of original CRC field
#define FILE_ORIG_CRC_SIZE 4            // Size of original CRC field
#define FILE_REGION_SIZE_OFFSET 72      // Offset of region size field
#define FILE_REGION_SIZE_SIZE 4         // Size of region size field

/**
 * @brief Firmware information structure
 * 
 * This structure stores information about a firmware file including its name,
 * location in flash memory, size, compression info, and checksums.
 */
struct firmware_info {
    char name[48];              /**< Firmware file name */
    uint16_t id;                /**< Firmware ID */
    uint16_t gzip;              /**< GZIP compression flag (1 if compressed, 0 if not) */
    uint32_t compressed_len;    /**< Compressed firmware size in bytes */
    uint32_t orig_length;       /**< Original firmware size in bytes */
    uint32_t addr;              /**< Flash address where firmware is stored */
    uint32_t data_crc;          /**< CRC32 checksum of compressed data */
    uint32_t data_oricrc;       /**< CRC32 checksum of original data */
    uint32_t region_size;       /**< Size of flash region allocated for this firmware */
};

// Use definitions from dfu_pan_macro.h
#define FIRMWARE_INFO_BASE_ADDR OTA_INFO_BASE_ADDR
#define VERSION_INFO_BASE_ADDR_FROM_MACRO VERSION_INFO_BASE_ADDR

/**
 * @brief Bluetooth application environment for OTA
 * 
 * This structure holds the Bluetooth connection state and related information
 * needed for over-the-air updates.
 */
typedef struct
{
    BOOL bt_connected;                   /**< Bluetooth connection status */
    bt_notify_device_mac_t bd_addr;       /**< Bluetooth device MAC address */
    rt_timer_t pan_connect_timer;         /**< Timer for PAN connection management */
} bt_app_t_ota;

extern bt_app_t_ota g_bt_app_env_ota;

/**
 * @brief Create OTA worker thread
 * 
 * Creates and starts a new thread to handle the OTA update process.
 * 
 * @param version_index Index of the version to update to
 * @return rt_err_t RT_EOK on success, error code otherwise
 */
rt_err_t dfu_pan_create_worker_thread(int version_index);

/**
 * @brief Print OTA files command
 * 
 * Prints information about the OTA firmware files to the console.
 */
void dfu_pan_print_files(void);

/**
 * @brief Parse OTA package from URL
 * 
 * Downloads and parses an OTA package from the specified URL.
 * 
 * @param package_url URL of the OTA package to download
 * @return int 0 on success, negative value on error
 */
int dfu_pan_parse_package_from_url(const char* package_url);

/**
 * @brief Query available OTA versions
 * 
 * Queries the server for available OTA versions.
 * 
 * @param server_url OTA server URL, if NULL uses default
 * @param current_version Current version to compare against
 * @return int Number of new versions found (0 if no new versions found), negative value on error
 */
int dfu_pan_query_versions(const char* server_url, const char* current_version);

/**
 * @brief Clear update flags
 * 
 * Clears the needs_update flag for all versions.
 */
void dfu_pan_clear_update_flags(void);

/**
 * @brief Get version info from flash
 * 
 * Reads version info from flash at specified index.
 * 
 * @param index Index of version info to read
 * @param version_info Pointer to struct to store the result
 * @return int 0 on success, negative value on error
 */
int dfu_pan_get_version_info(int index, struct version_info *version_info);

/**
 * @brief Get firmware info from flash
 * 
 * Reads firmware info from flash at specified index.
 * 
 * @param index Index of firmware info to read
 * @param firmware_info Pointer to struct to store the result
 * @return int 0 on success, negative value on error
 */
int dfu_pan_get_firmware_info(int index, struct firmware_info *firmware_info);

/**
 * @brief Test update flags functionality
 * 
 * This function provides a complete test of the update flags mechanism:
 * 1. Reads and displays the initial state of all version entries
 * 2. Sets all needs_update flags to 1 and writes to flash
 * 3. Verifies that all flags are correctly set to 1
 * 4. Clears all needs_update flags to 0 and writes to flash
 * 5. Verifies that all flags are correctly cleared to 0
 * 
 * This test helps verify:
 * - Correct address calculation for each version entry
 * - Proper flash read/write operations
 * - Accurate flag setting and clearing functionality
 * - Bootloader's ability to correctly identify update flags
 */
void dfu_pan_test_update_flags(void);
#endif