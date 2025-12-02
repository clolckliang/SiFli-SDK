# DFU_PAN OTA Upgrade User Guide

## 1. Overview

DFU_PAN is an OTA firmware upgrade middleware based on Bluetooth PAN network. It enables devices to connect to OTA servers via Bluetooth network, download and update firmware. The entire process includes device registration, version checking, firmware downloading, and update steps.

## 2. Workflow Overview

```
1. Device Startup → 2. Device Registration → 3. Version Check → 4. Set Update Flag → 5. Reboot to Enter OTA Mode
     ↓
6. DFU_PAN Program Execution → 7. Firmware Download → 8. Firmware Verification → 9. Reboot to Enter Normal Mode
```

## 3. Detailed Usage Steps

### 3.1 Device Registration Process

Devices need to register with the OTA server during the first connection to allow the server to identify the device and record device information.

**Implementation**: dfu_pan provides a registration interface, and the application needs to prepare device information (optional parameters can be omitted).

```c
// Device registration request parameters structure
typedef struct {
    const char* mac;          // MAC address (required)
    const char* model;        // Device model (required)
    const char* solution;     // Solution name (required)
    const char* version;      // Current version (required)
    const char* ota_version;  // OTA version (required)
    const char* screen_width; // Screen width (optional)
    const char* screen_height;// Screen height (optional)
    const char* flash_type;   // Flash type (optional)
    const char* chip_id;      // Chip ID (required)
} device_register_params_t;
```

**Process**:
1. Construct device registration parameters:
   - MAC address
   - Device model
   - Solution name
   - Current version number
   - OTA version number
   - Chip ID (unique ID generated using SHA256)
- ota_server_url: https://xxx.xxx.com

2. Call `dfu_pan_register_device(ota_server_url, &reg_params)`
- The API for POST request inside dfu_pan_register_device: https://xxx.xxx.com/register

Example:
```c
// Construct registration parameters
device_register_params_t reg_params = {0};
reg_params.mac = get_mac_address();
reg_params.model = "sf32lb52-lchspi-ulp";
reg_params.solution = "SF32LB52_ULP_NOR_TFT_CO5300";
reg_params.version = VERSION;
reg_params.ota_version = VERSION;
reg_params.chip_id = get_client_id();

// Execute registration
int result = dfu_pan_register_device("https://xxx.xxx.com", &reg_params);
```
json:
```json
{
      "mac": "required", 
      "model": "required",
      "solution": "required",
      "version": "required",
      "ota_version": "required",
      "screen_width": "optional",
      "screen_height": "optional",
      "flash_type": "optional",
      "chip_id": "required"
    }
```

**Note**: The `solution` and `model` fields should preferably be consistent with the folder structure. For example, if the folder structure is `"https://xxx.xxx.com/v2/xiaozhi/SF32LB52_ULP_NOR_TFT_CO5300/sf32lb52-lchspi-ulp?chip_id=%s&version=latest"`

### 3.2 Version Check Process

After successful registration, the device can query the server for new firmware versions.

**Process**:
1. Build the query URL containing the device's chip_id and current version
2. Call `dfu_pan_query_latest_version()` to query the latest version
3. If a new version is available, save the firmware information to Flash

```c
// Build query URL
char* chip_id = get_client_id();
char* dynamic_ota_url = build_ota_query_url(chip_id);
// Query latest version
int result = dfu_pan_query_latest_version(dynamic_ota_url, VERSION, 
                                       latest_version, sizeof(latest_version));
```
`build_ota_query_url` needs to be implemented on the application side to construct the URL.
Example:
```c
dynamic_ota_url: "https://xxx.xxx.com/v2/xiaozhi/SF32LB52_ULP_NOR_TFT_CO5300/sf32lb52-lchspi-ulp?chip_id=%s&version=latest"
// SF32LB52_ULP_NOR_TFT_CO5300/sf32lb52-lchspi-ulp is based on the server file deployment hierarchy
```

**Note**: dfu_pan_query_latest_version() will obtain the JSON data returned by the server and write the firmware structure to the Flash address for direct download later. The JSON is as follows, where v1.3.9.bin is a placeholder file that needs to be placed together with the firmware files and have the same name as the current folder.
```json
{
  "result": 200,
  "message": "Success",
  "data": [
    {
      "name": "v1.3.9",
      "thumb": null,
      "zippath": "https://xxx.xxx.com/download?path=xiaozhi/SF32LB52_ULP_NOR_TFT_CO5300/sf32lb52-lchspi-ulp/v1.3.9/v1.3.9.bin",
      "files": [
        {
          "file_id": 391,
          "file_name": "ER_IROM3.bin",
          "file_size": 4087608,
          "crc32": "0x1026622b",
          "addr": "0x12460000",
          "region_size": "0x00680000",
          "note": null,
          "name": "ER_IROM3.bin",
          "url": "https://xxx.xxx.com/download?path=xiaozhi/SF32LB52_ULP_NOR_TFT_CO5300/sf32lb52-lchspi-ulp/v1.3.9/ER_IROM3.bin"
        },
        {
          "name": "v1.3.9.bin",
          "url": "https://xxx.xxx.com/download?path=xiaozhi/SF32LB52_ULP_NOR_TFT_CO5300/sf32lb52-lchspi-ulp/v1.3.9/v1.3.9.bin",
          "addr": "",
          "size": "",
          "crc32": "",
          "region_size": "",
          "note": ""
        },
        {
          "file_id": 389,
          "file_name": "ER_IROM1.bin",
          "file_size": 2036952,
          "crc32": "0xbffebc91",
          "addr": "0x12218000",
          "region_size": "0x00240000",
          "note": null,
          "name": "ER_IROM1.bin",
          "url": "https://xxx.xxx.com/download?path=xiaozhi/SF32LB52_ULP_NOR_TFT_CO5300/sf32lb52-lchspi-ulp/v1.3.9/ER_IROM1.bin"
        },
        {
          "file_id": 390,
          "file_name": "ER_IROM2.bin",
          "file_size": 3939852,
          "crc32": "0x32d58690",
          "addr": "0x12AE0000",
          "region_size": "0x00400000",
          "note": null,
          "name": "ER_IROM2.bin",
          "url": "https://xxx.xxx.com/download?path=xiaozhi/SF32LB52_ULP_NOR_TFT_CO5300/sf32lb52-lchspi-ulp/v1.3.9/ER_IROM2.bin"
        }
      ]
    }
  ]
}
```

**Firmware Information Structure**:

```c
struct firmware_file_info {
    char name[48];         // File name
    char url[256];         // Download URL
    uint32_t addr;         // Flash address
    uint32_t size;         // File size
    uint32_t crc32;        // CRC32 checksum
    uint32_t region_size;  // Region size
    uint32_t file_id;      // File ID
    uint32_t needs_update; // Update flag
    uint32_t magic;        // Magic number
};
```

### 3.3 Set Update Flag

When a new version is detected, an update flag needs to be set to enter OTA mode at the next startup.

**Process**:
1. The user clicks the "Update" button on the UI or triggers the update operation at a specific location
2. Call `dfu_pan_set_update_flags()` to set the update flag
3. System reboot

```c
// Set update flag
if (dfu_pan_set_update_flags() != 0) {
    LOG_E("Failed to mark versions for update");
    return;
}

// Reboot the system, and the bootloader will enter the OTA download program by judging the magic number and update flag bit
HAL_PMU_Reboot();
```
```c 
// dfu_pan_set_update_flags will set `magic` and `needs_update` in `firmware_file_info` respectively
// Magic number definitions
#define FIRMWARE_INFO_MAGIC                                                    \
    0x64667500 // ASCII value of "dfu", ensuring 4-byte alignment
#define FIRMWARE_INFO_MAGIC_PAN                                                \
    0x70616E00 // ASCII value of "pan", ensuring 4-byte alignment
#define FIRMWARE_MAGIC_DFU_PAN                                                 \
    ((uint32_t)FIRMWARE_INFO_MAGIC << 16 | (FIRMWARE_INFO_MAGIC_PAN & 0xFFFF))

    needs_update = 1
```

### 3.4 Bootloader Check Update Flag

After the system reboots, the bootloader will check for the update flag.

**Process**:
1. Check the firmware information stored in Flash
2. Verify the magic number and update flag
3. If an update is required and the OTA program is valid, jump to the OTA program

```c
// Check update flag
bool needs_update = 0;
for (int i = 0; i < MAX_FIRMWARE_FILES; i++) {
    // Check magic number
    uint32_t magic_value = 0;
    g_flash_read(magic_addr, (const int8_t*)&magic_value, sizeof(uint32_t));
    
    if (magic_value == FIRMWARE_MAGIC_DFU_PAN) {
        // Check update flag
        uint32_t needs_update_value = 0;
        g_flash_read(needs_update_addr, (const int8_t*)&needs_update_value, 
                     sizeof(uint32_t));
        
        if (needs_update_value) {
            needs_update = 1;
            break;
        }
    }
}

// If update is needed and OTA program is valid, jump
if (needs_update && is_ota_program_valid(DFU_PAN_LOADER_START_ADDR)) {
    run_img(DFU_PAN_LOADER_START_ADDR);  // Jump to OTA program
}
```

### 3.5 DFU_PAN Program Executes Firmware Download

After entering OTA mode, the DFU_PAN program will automatically connect to the network and download the firmware.

**Process**:
1. Initialize Bluetooth and connect to the PAN network
2. Automatically check network connection status
3. Read firmware information from Flash
4. Call `dfu_pan_download_firmware()` to download firmware
5. Clear the update flag and reboot after successful download

```c
// Read firmware information
struct firmware_file_info firmware_files[MAX_FIRMWARE_FILES];
int file_count = 0;
for (int i = 0; i < MAX_FIRMWARE_FILES; i++) {
    if (dfu_pan_get_firmware_file_info(i, &firmware_files[file_count]) == 0) {
        if (firmware_files[file_count].name[0] != '\0') {
            file_count++;
        }
    }
}

// Download firmware
int ret = dfu_pan_download_firmware(firmware_files, file_count);
if (ret == 0) {
    dfu_pan_clear_files();  // Clear update flag
    HAL_PMU_Reboot();       // Reboot to enter normal mode
}
```

### 3.6 Firmware Download and Verification Process

**Process**:
1. Traverse all firmware files that need to be updated
2. For each file:
   - Erase the target Flash region
   - Download firmware via HTTP GET request
   - Write to Flash in chunks
   - Display download progress
   - Calculate and verify CRC32 checksum

```c
// Download each firmware file
for (int i = 0; i < file_count; i++) {
    // Erase Flash
    rt_flash_erase(firmware_file_info[i].addr, aligned_size);
    
    // Create HTTP session
    session = webclient_session_create(PAN_OTA_HEADER_BUFSZ);
    
    // Send GET request
    int resp_status = webclient_get(session, firmware_file_info[i].url);
    
    // Download in chunks and write to Flash
    while (remaining_length > 0) {
        int bytes_read = webclient_read(session, buffer, chunk_size);
        rt_flash_write(addr, (uint8_t *)buffer, bytes_read);
    }
    
    // CRC verification
    uint32_t calculated_crc = calculate_crc32(verify_buffer, verify_chunk, 
                                             calculated_crc);
    if (calculated_crc != firmware_file_info[i].crc32) {
        // Verification failed
        return -1;
    }
}
```

## 4. CRC32 Checksum Algorithm Description

The DFU_PAN OTA upgrade process uses the CRC32 checksum algorithm to ensure the integrity of firmware data. The algorithm implementation is consistent with the standard CRC32 (IEEE 802.3), and the specific logic is as follows:

### 4.1 CRC32 Algorithm Parameters

- **Polynomial**: 0xEDB88320 (standard IEEE 802.3 CRC32 polynomial)
- **Initial Value**: 0xFFFFFFFF
- **Final XOR Value**: 0xFFFFFFFF
- **Input Data Reversal**: No
- **Output Data Reversal**: No

### 4.2 Algorithm Implementation

#### 4.2.1 CRC32 Table Initialization

```c
#define CRC32_POLY 0xEDB88320
static uint32_t crc32_table[256];

static void init_crc32_table(void)
{
    for (int i = 0; i < 256; i++)
    {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++)
        {
            if (crc & 1)
                crc = (crc >> 1) ^ CRC32_POLY;
            else
                crc >>= 1;
        }
        crc32_table[i] = crc;
    }
}
```

#### 4.2.2 CRC32 Calculation Function

```c
static uint32_t calculate_crc32(const uint8_t *data, size_t length, uint32_t crc)
{
    static int crc_table_initialized = 0;

    // Ensure CRC table is initialized only once
    if (!crc_table_initialized)
    {
        init_crc32_table();
        crc_table_initialized = 1;
    }

    crc = crc ^ 0xFFFFFFFF;
    for (size_t i = 0; i < length; i++)
    {
        crc = (crc >> 8) ^ crc32_table[(crc ^ data[i]) & 0xFF];
    }
    return crc ^ 0xFFFFFFFF;
}
```

### 4.3 Algorithm Workflow

1. **Table Initialization**: Initialize the 256-entry CRC32 lookup table on the first call
2. **Initial Value Processing**: XOR the initial CRC value with 0xFFFFFFFF
3. **Data Processing**: For each byte of input data:
   - Calculate index: `(current CRC value ^ byte value) & 0xFF`
   - Update CRC: `(current CRC value >> 8) ^ crc32_table[index]`
4. **Final Value Processing**: XOR the result with 0xFFFFFFFF to get the final CRC32 value

### 4.4 Application in Firmware Verification

After the firmware download is completed, the system performs CRC32 verification to confirm firmware integrity:

```c
// Read downloaded firmware data from Flash
uint8_t *verify_buffer = rt_malloc(OTA_RECV_BUFF_SIZE);
uint32_t calculated_crc = 0xffffffff;
uint32_t verify_remaining = data_written; // Actual written data size
uint32_t verify_offset = 0;

// Read in chunks and calculate CRC32
while (verify_remaining > 0)
{
    int verify_chunk = (verify_remaining > OTA_RECV_BUFF_SIZE) 
                          ? OTA_RECV_BUFF_SIZE 
                          : verify_remaining;

    if (rt_flash_read(firmware_file_info[i].addr + verify_offset,
                      verify_buffer, verify_chunk) != verify_chunk)
    {
        // Read failure handling
        rt_free(verify_buffer);
        return -1;
    }

    // Accumulate CRC32 calculation
    calculated_crc = calculate_crc32(verify_buffer, verify_chunk, calculated_crc);

    verify_remaining -= verify_chunk;
    verify_offset += verify_chunk;
}

// Compare calculated CRC with server-provided CRC value
if (calculated_crc != firmware_file_info[i].crc32)
{
    // CRC verification failed, firmware may be corrupted
    return -1;
}
```

### 4.5 Notes

1. **Consistency Requirement**: The CRC32 value generated by the server must use the same algorithm and parameters
2. **Data Integrity**: The entire firmware file must be completely downloaded before CRC verification can be performed
3. **Error Handling**: The update process should be terminated and an error reported when CRC verification fails
4. **Performance Optimization**: Use the lookup table method to improve calculation efficiency and avoid bitwise operations for each calculation

Through the above CRC32 verification mechanism, DFU_PAN can effectively ensure the integrity and correctness of firmware data during the OTA upgrade process, preventing firmware corruption caused by transmission errors.