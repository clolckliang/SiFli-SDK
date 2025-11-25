#include "rtthread.h"
#include <webclient.h>
#include <cJSON.h>
#include <ulog.h>
#include <lwip/sys.h>
#include "lwip/tcpip.h"
#include "string.h"
#include <stdlib.h>
#include <ctype.h>
#include "register.h"
#include "bt_pan_ota.h"
#include "dfu_pan_ui.h"

#define OTA_WORKER_THREAD_STACK_SIZE 4096
static struct rt_thread ota_worker_thread;
static uint8_t
    ota_worker_stack[OTA_WORKER_THREAD_STACK_SIZE] ALIGN(RT_ALIGN_SIZE);

bt_app_t_ota g_bt_app_env_ota;

#define OTA_LOCAL_HOST "0.0.0.0:8000"
#define OTA_SERVER_HOST "ota.sifli.com"
#define PAN_OTA_HEADER_BUFSZ 1024
#define OTA_RECV_BUFF_SIZE 1024

// Definitions related to CRC32 checksum
#define CRC32_POLY 0xEDB88320
static uint32_t crc32_table[256];

// Initialize the CRC32 table
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

// Make sure to initialize the table before using CRC.
static uint32_t calculate_crc32(const uint8_t *data, size_t length,
                                uint32_t crc)
{
    static int crc_table_initialized = 0;

    // 确保CRC表只初始化一次
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

static void exist_sniff_mode(void)
{
    rt_kprintf("exit sniff mode\n");
    bt_interface_exit_sniff_mode(
        (unsigned char *)&g_bt_app_env_ota.bd_addr); // exit sniff mode
    bt_interface_wr_link_policy_setting(
        (unsigned char *)&g_bt_app_env_ota.bd_addr,
        BT_NOTIFY_LINK_POLICY_ROLE_SWITCH); // close role switch
}
static int dfu_pan_main_with_version(int version_index)
{
    LOG_I("Starting automatic firmware download and installation for version %d...\n", version_index);

    if (version_index >= 0 && version_index < MAX_VERSION_COUNT)
    {
        struct version_info version_info;
        if (dfu_pan_get_version_info(version_index, &version_info) != 0) {
            LOG_E("Failed to get version info at index %d\n", version_index);
            return -1;
        }
        
        LOG_I("Version name: %s\n", version_info.name);
        LOG_I("URL: %s\n", version_info.zippath);
        
        int result = dfu_pan_parse_package_from_url(version_info.zippath);

        if (result == 0)
        {
            // Update successful. Clear the flag status.
            dfu_pan_clear_update_flags();
            rt_thread_mdelay(50);
            dfu_pan_ui_update_message(UI_MSG_SHOW_SUCCESS_POPUP, NULL);
        }
        else
        {
            dfu_pan_ui_update_message(UI_MSG_SHOW_FAILURE_POPUP, NULL);
        }
    }
    else
    {
        LOG_E("Invalid version index: %d\n", version_index);
        return -1;
    }

    return 0;
}

static void dfu_pan_worker_thread_entry(void *parameter)
{

    int version_index = (int)(uintptr_t)parameter;

    dfu_pan_main_with_version(version_index);
}

static int compare_version_strings(const char* v1, const char* v2)
{
    if (!v1 || !v2) {
        return -2;
    }
    
    BOOL v1_has_prefix = (v1[0] == 'v' || v1[0] == 'V');
    BOOL v2_has_prefix = (v2[0] == 'v' || v2[0] == 'V');
    
    if (v1_has_prefix != v2_has_prefix) {
        return -2;
    }
    
    char version1[32];
    char version2[32];
    
    const char* ver1_ptr = v1;
    const char* ver2_ptr = v2;
    
    if (v1_has_prefix) {
        ver1_ptr++;
    }
    
    if (v2_has_prefix) {
        ver2_ptr++;
    }
    
    if (*ver1_ptr == '\0' || *ver2_ptr == '\0') {
        return -2;
    }
    
    strncpy(version1, ver1_ptr, sizeof(version1) - 1);
    version1[sizeof(version1) - 1] = '\0';
    
    strncpy(version2, ver2_ptr, sizeof(version2) - 1);
    version2[sizeof(version2) - 1] = '\0';

    char *temp1 = version1;
    char *temp2 = version2;
    char *token1, *token2;
    
    while (1) {
        token1 = strtok_r(temp1, ".", &temp1);
        token2 = strtok_r(temp2, ".", &temp2);
        
        if (token1 == NULL && token2 == NULL) {
            return 0;
        }
        
        int num1 = token1 ? atoi(token1) : 0;
        int num2 = token2 ? atoi(token2) : 0;
        
        if (num1 > num2) return 1;
        if (num1 < num2) return -1;
        
        if (token1 == NULL || token2 == NULL) {
            continue;
        }
    }
}

int dfu_pan_query_versions(const char* server_url, const char* current_version)
{
    struct webclient_session *session = RT_NULL;
    char *buffer = RT_NULL;
    struct version_info *temp_version_list = RT_NULL;
    int content_pos = 0;
    int resp_status = 0;
    const char *query_url = server_url;
    int ret = 0;
    int new_version_count = 0; // Used to record the number of new versions

    LOG_I("dfu_pan_query_versions\n");

    if (server_url == NULL) {
        LOG_W("Server URL is NULL!\n");
        return -1;
    }
    
    rt_kputs(server_url);
    LOG_I("\n");

    temp_version_list =
        rt_calloc(MAX_VERSION_COUNT, sizeof(struct version_info));
    if (!temp_version_list)
    {
        LOG_E("No memory for version list!");
        ret = -1;
        goto __exit;
    }

    LOG_I("exist_sniff_mode\n");
    exist_sniff_mode();

    session = webclient_session_create(PAN_OTA_HEADER_BUFSZ);
    if (!session)
    {
        LOG_E("Create session failed!");
        ret = -1;
        goto __exit;
    }

    buffer = rt_calloc(1, OTA_RECV_BUFF_SIZE);
    if (!buffer)
    {
        LOG_E("No memory for buffer!");
        ret = -1;
        goto __exit;
    }

    resp_status = webclient_get(session, query_url);
    if (resp_status != 200)
    {
        LOG_E("GET request failed, response code: %d", resp_status);
        ret = -1;
        goto __exit;
    }

    //Check whether the length of the response content exceeds the buffer size.
    if (session->content_length > OTA_RECV_BUFF_SIZE) {
        LOG_E("Response content length (%d) exceeds buffer size (%d)", 
              session->content_length, OTA_RECV_BUFF_SIZE);
        ret = -1;
        goto __exit;
    }

    while (content_pos < session->content_length)
    {
        int bytes_to_read = session->content_length - content_pos > OTA_RECV_BUFF_SIZE
                              ? OTA_RECV_BUFF_SIZE
                              : session->content_length - content_pos;
                              
        // Add additional security checks
        if (content_pos + bytes_to_read > OTA_RECV_BUFF_SIZE) {
            bytes_to_read = OTA_RECV_BUFF_SIZE - content_pos;
        }
        
        if (bytes_to_read <= 0) {
            LOG_E("Buffer overflow prevented. Would read %d bytes to offset %d", 
                  bytes_to_read, content_pos);
            ret = -1;
            goto __exit;
        }
                              
        int bytes_read = webclient_read(
            session, buffer + content_pos,
            bytes_to_read);
        if (bytes_read <= 0)
        {
            break;
        }
        content_pos += bytes_read;
    }


    if (content_pos > 0)
    {

        cJSON *root = cJSON_Parse(buffer);
        if (!root)
        {
            LOG_E("Failed to parse JSON: [%s]", cJSON_GetErrorPtr());
            ret = -1;
            goto __exit;
        }

        LOG_I("Version check response:\n");
        rt_kputs(buffer);
        LOG_I("\n");

        cJSON *result_item = cJSON_GetObjectItem(root, "result");
        if (!result_item || result_item->valueint != 200)
        {
            LOG_E("Server returned error result: %d",
                  result_item ? result_item->valueint : -1);
            cJSON_Delete(root);
            ret = -1;
            goto __exit;
        }

        cJSON *data_array = cJSON_GetObjectItem(root, "data");
        if (!data_array || !cJSON_IsArray(data_array))
        {
            LOG_E("Invalid data format in response");
            cJSON_Delete(root);
            ret = -1;
            goto __exit;
        }

        // Clear the old version information
        rt_memset(temp_version_list, 0,
                  sizeof(struct version_info) * MAX_VERSION_COUNT);

        int version_count = 0;
        cJSON *version_item = NULL;
        cJSON_ArrayForEach(version_item, data_array)
        {
            if (version_count >= MAX_VERSION_COUNT)
            {
                LOG_W("Too many versions, only process first %d",
                      MAX_VERSION_COUNT);
                break;
            }

            cJSON *name_item = cJSON_GetObjectItem(version_item, "name");
            cJSON *zippath_item = cJSON_GetObjectItem(version_item, "zippath");

            if (name_item && cJSON_IsString(name_item) && zippath_item &&
                cJSON_IsString(zippath_item))
            {
                // Only add to the list when the version is higher than the current version.
                if (current_version && compare_version_strings(name_item->valuestring, current_version) <= 0) {
                    LOG_I("Skipping version %s as it's not higher than current version %s", 
                          name_item->valuestring, current_version);
                    continue;
                }

                new_version_count++; // Record the number of new versions

                rt_strncpy(temp_version_list[version_count].name,
                           name_item->valuestring,
                           sizeof(temp_version_list[version_count].name) - 1);

                // Replace the domain name in the URL
                char *zippath = zippath_item->valuestring;
                if (strstr(zippath, OTA_LOCAL_HOST))
                {

                    char *temp_path = rt_malloc(256);
                    if (temp_path)
                    {
                        char *pos = strstr(zippath, OTA_LOCAL_HOST);
                        int prefix_len = pos - zippath;
                        int suffix_len = strlen(pos + strlen(OTA_LOCAL_HOST));
                        if (prefix_len + strlen(OTA_SERVER_HOST) + suffix_len <
                            256)
                        {
                            rt_memcpy(temp_path, zippath, prefix_len);
                            rt_memcpy(temp_path + prefix_len, OTA_SERVER_HOST,
                                      strlen(OTA_SERVER_HOST));
                            rt_memcpy(temp_path + prefix_len +
                                          strlen(OTA_SERVER_HOST),
                                      pos + strlen(OTA_LOCAL_HOST),
                                      suffix_len + 1);
                            rt_strncpy(
                                temp_version_list[version_count].zippath,
                                temp_path,
                                sizeof(
                                    temp_version_list[version_count].zippath) -
                                    1);
                        }
                        else
                        {
                            rt_strncpy(
                                temp_version_list[version_count].zippath,
                                zippath,
                                sizeof(
                                    temp_version_list[version_count].zippath) -
                                    1);
                        }
                        rt_free(temp_path);
                    }
                    else
                    {
                        rt_strncpy(
                            temp_version_list[version_count].zippath, zippath,
                            sizeof(temp_version_list[version_count].zippath) -
                                1);
                    }
                }
                else
                {
                    rt_strncpy(
                        temp_version_list[version_count].zippath, zippath,
                        sizeof(temp_version_list[version_count].zippath) - 1);
                }
                // Set the magic number
                temp_version_list[version_count].magic = VERSION_MAGIC_DFU_PAN;

                LOG_I("Found version--- %d: ", version_count);
                LOG_I("name: %s\n", temp_version_list[version_count].name);
                rt_kputs(temp_version_list[version_count].zippath);
                LOG_I("Magic: %s\n",(temp_version_list[version_count].magic == VERSION_MAGIC_DFU_PAN)
                ? "Valid": "Invalid");
                LOG_I("---\n");

                version_count++;
            }
        }

        // Write the version information into Flash
        int erase_alignment =
            rt_flash_get_erase_alignment(VERSION_INFO_BASE_ADDR);
        int data_size = sizeof(struct version_info) * MAX_VERSION_COUNT;
        int aligned_size =
            ((data_size + erase_alignment - 1) / erase_alignment) *
            erase_alignment;

        if (rt_flash_erase(VERSION_INFO_BASE_ADDR, aligned_size) != RT_EOK)
        {
            LOG_E("Failed to erase flash at 0x%08X", VERSION_INFO_BASE_ADDR);
            cJSON_Delete(root);
            ret = -1;
            goto __exit;
        }

        // Mark all versions for update before writing to flash
        LOG_I("Marking all versions for update");
        for (int i = 0; i < version_count; i++)
        {
            temp_version_list[i].needs_update = 1;
            LOG_I("Marked version[%d] %s for update", i,
                  temp_version_list[i].name);
        }

        // Write version information with update flags to flash
        if (rt_flash_write(VERSION_INFO_BASE_ADDR, (uint8_t *)temp_version_list,
                           data_size) != data_size)
        {
            LOG_E("Failed to write version info to flash");
            cJSON_Delete(root);
            ret = -1;
            goto __exit;
        }

        LOG_I("Successfully saved %d version info to flash", version_count);

        // Verify the data that has been written
        struct version_info *verify_list =
            rt_calloc(MAX_VERSION_COUNT, sizeof(struct version_info));
        if (verify_list)
        {
            int read_result = rt_flash_read(VERSION_INFO_BASE_ADDR,
                                            (uint8_t *)verify_list, data_size);
            LOG_I("Flash read result: %d (expected: %d)", read_result,
                  data_size);

            for (int i = 0; i < version_count; i++)
            {
                LOG_I("Verify version[%d]: %s, needs_update: %d", i,
                      verify_list[i].name, verify_list[i].needs_update);
            }
            rt_free(verify_list);
        }

        cJSON_Delete(root);
    }
    else
    {
        LOG_E("Failed to read response data. Content pos: %d", content_pos);
        LOG_E("Session content length: %d", session->content_length);
        ret = -1;
    }

__exit:
    if (temp_version_list)
    {
        rt_free(temp_version_list);
    }

    if (session)
    {
        LOCK_TCPIP_CORE();
        webclient_close(session);
        UNLOCK_TCPIP_CORE();
    }

    if (buffer)
    {
        rt_free(buffer);
    }

    // Return the corresponding value based on the result.
    if (ret == 0) {
        return new_version_count; // Return the number of new versions
    }

    return ret; // Return -1 in case of an error.
}

// Parse the firmware package (streaming version) downloaded from the URL
// This function uses streaming processing to avoid memory overflow issues
int dfu_pan_parse_package_from_url(const char *package_url)
{

    struct webclient_session *session = RT_NULL;
    char *buffer = RT_NULL;
    int content_pos = 0;
    int resp_status = 0;
    int ret = 0;
    int package_size = 0;

    LOG_I("Starting to download and parse OTA package from:\n");
    rt_kputs(package_url);
    LOG_I("\n");

    LOG_E("exist_sniff_mode\n");
    exist_sniff_mode();

    // Create network session
    session = webclient_session_create(PAN_OTA_HEADER_BUFSZ);
    if (!session)
    {
        LOG_E("Create session failed!\n");
        ret = -1;
        goto __exit;
    }

    // Allocate receive buffer
    buffer = rt_calloc(1, OTA_RECV_BUFF_SIZE);
    if (!buffer)
    {
        LOG_E("No memory for buffer!\n");
        ret = -1;
        goto __exit;
    }

    // Send GET request to download firmware package
    resp_status = webclient_get(session, package_url);
    LOG_I("HTTP response status: %d\n", resp_status);

    const char *content_type =
        webclient_header_fields_get(session, "Content-Type");
    if (content_type)
    {
        LOG_I("Content-Type: %s\n", content_type);
    }

    const char *content_length =
        webclient_header_fields_get(session, "Content-Length");
    if (content_length)
    {
        LOG_I("Content-Length: %s\n", content_length);
    }

    // Check HTTP response status
    if (resp_status != 200)
    {
        LOG_E("GET request failed, response code: %d\n", resp_status);
        ret = -1;
        goto __exit;
    }

    // Get package size
    package_size = session->content_length;
    LOG_I("Package size: %d bytes\n", package_size);

    if (package_size <= 0)
    {
        LOG_E("Invalid package size: %d\n", package_size);
        ret = -1;
        goto __exit;
    }

    uint32_t headers_crc, magic, version, bin_num;
    int header_size = 0;
    int data_offset = 0;

    // Read the first 16 bytes of the package header
    int initial_bytes_read = webclient_read(session, buffer, BASE_HEADER_SIZE);
    LOG_I("Initial bytes read: %d\n", initial_bytes_read);

    // Print first 16 bytes of data for debugging
    LOG_I("First 16 bytes of data: ");
    for (int i = 0; i < (initial_bytes_read < BASE_HEADER_SIZE ? initial_bytes_read : BASE_HEADER_SIZE);
         i++)
    {
        LOG_I("%02X ", (unsigned char)buffer[i]);
    }
    LOG_I("\n");

    // Check if data was successfully read
    if (initial_bytes_read <= 0)
    {
        LOG_E("Failed to read package header\n");
        ret = -1;
        goto __exit;
    }

    if (initial_bytes_read < BASE_HEADER_SIZE)
    {
        LOG_E(
            "Insufficient data read for package header. Expected %d, got %d\n",
            BASE_HEADER_SIZE, initial_bytes_read);
        ret = -1;
        goto __exit;
    }

    // Parse basic header information
    rt_memcpy(&headers_crc, buffer + HEADER_CRC_OFFSET, HEADER_CRC_SIZE);
    rt_memcpy(&magic, buffer + HEADER_MAGIC_OFFSET, HEADER_MAGIC_SIZE);
    rt_memcpy(&version, buffer + HEADER_VERSION_OFFSET, HEADER_VERSION_SIZE);
    rt_memcpy(&bin_num, buffer + HEADER_BIN_NUM_OFFSET, HEADER_BIN_NUM_SIZE);

    LOG_I("Package header - CRC: 0x%08X, Magic: 0x%08X, Version: 0x%08X, Bin "
          "Count: %d\n",
          headers_crc, magic, version, bin_num);

    // Verify magic number (consistent with magic number used in Python packaging script)
    if (magic != OTA_PACKAGE_MAGIC)
    {
        LOG_E("Invalid package magic number. Expected: 0x%08X, Got: 0x%08X\n",
              OTA_PACKAGE_MAGIC, magic);
        ret = -1;
        goto __exit;
    }

    // Check if the number of bin files is reasonable
    if (bin_num > MAX_FIRMWARE_FILES || bin_num == 0)
    {
        LOG_E("Invalid bin file count: %d. MAX_FIRMWARE_FILES is %d\n", bin_num,
              MAX_FIRMWARE_FILES);
        ret = -1;
        goto __exit;
    }

    // Calculate the size of each bin file header: 48 + 2 + 2 + 4 + 4 + 4 + 4 + 4 + 4 = 76 bytes
    int per_file_header_size = PER_FILE_HEADER_SIZE;
    header_size =
        BASE_HEADER_SIZE + bin_num * per_file_header_size; // 16-byte base header + 76-byte header for each bin file

    // Create a temporary array to store firmware information
    struct firmware_info temp_firmware_files[MAX_FIRMWARE_FILES];
    rt_memset(temp_firmware_files, 0, sizeof(temp_firmware_files));

    int last_sent_progress[MAX_FIRMWARE_FILES];
    for (int j = 0; j < MAX_FIRMWARE_FILES; j++)
    {
        last_sent_progress[j] = -1;
    }
    
    // 2. Read and parse the header information of each bin file
    for (int i = 0; i < bin_num; i++)
    {
        // Read the header (76 bytes) of a bin file
        int bytes_read = 0;
        int total_read = 0;
        while (total_read < per_file_header_size &&
               (bytes_read =
                    webclient_read(session, buffer + total_read,
                                   per_file_header_size - total_read)) > 0)
        {
            total_read += bytes_read;
        }

        // Check if the header information was completely read
        if (total_read != per_file_header_size)
        {
            LOG_E("Failed to read bin header %d, read %d bytes, expected %d\n",
                  i, total_read, per_file_header_size);
            ret = -1;
            goto __exit;
        }

        // Parse the header information of the bin file (a total of 76 bytes)
        char name[49];
        uint16_t id, gzip;
        uint32_t compressed_len, orig_len, addr, data_crc, data_oricrc,
            region_size;

        rt_memcpy(name, buffer, FILE_NAME_FIELD_SIZE);
        name[FILE_NAME_SIZE] = '\0';
        rt_memcpy(&id, buffer + FILE_ID_OFFSET, FILE_ID_SIZE);
        rt_memcpy(&gzip, buffer + FILE_GZIP_OFFSET, FILE_GZIP_SIZE);
        rt_memcpy(&compressed_len, buffer + FILE_COMPRESSED_LEN_OFFSET, FILE_COMPRESSED_LEN_SIZE);
        rt_memcpy(&orig_len, buffer + FILE_ORIG_LEN_OFFSET, FILE_ORIG_LEN_SIZE);
        rt_memcpy(&addr, buffer + FILE_ADDR_OFFSET, FILE_ADDR_SIZE);
        rt_memcpy(&data_crc, buffer + FILE_CRC_OFFSET, FILE_CRC_SIZE);
        rt_memcpy(&data_oricrc, buffer + FILE_ORIG_CRC_OFFSET, FILE_ORIG_CRC_SIZE);
        rt_memcpy(&region_size, buffer + FILE_REGION_SIZE_OFFSET, FILE_REGION_SIZE_SIZE);

        LOG_I("Bin file %d:\n", i);
        LOG_I("  Name: %s\n", name);
        LOG_I("  ID: %d\n", id);
        LOG_I("  GZIP: %d\n", gzip);
        LOG_I("  Compressed Length: %d\n", compressed_len);
        LOG_I("  Original Length: %d\n", orig_len);
        LOG_I("  Address: 0x%08X\n", addr);
        LOG_I("  Data CRC: 0x%08X\n", data_crc);
        LOG_I("  Original CRC: 0x%08X\n", data_oricrc);
        LOG_I("  Region Size: 0x%08X\n", region_size);

        rt_strncpy(temp_firmware_files[i].name, name,
                   sizeof(temp_firmware_files[i].name) - 1);
        temp_firmware_files[i].id = id;
        temp_firmware_files[i].gzip = gzip;
        temp_firmware_files[i].compressed_len = compressed_len;
        temp_firmware_files[i].orig_length = orig_len;
        temp_firmware_files[i].addr = addr;
        temp_firmware_files[i].data_crc = data_crc;
        temp_firmware_files[i].data_oricrc = data_oricrc;
        temp_firmware_files[i].region_size = region_size;
    }

    int erase_alignment = rt_flash_get_erase_alignment(OTA_INFO_BASE_ADDR);
    int data_size = sizeof(struct firmware_info) * MAX_FIRMWARE_FILES;
    int aligned_size =
        ((data_size + erase_alignment - 1) / erase_alignment) * erase_alignment;

    // Erase flash area for firmware info
    if (rt_flash_erase(OTA_INFO_BASE_ADDR, aligned_size) != RT_EOK)
    {
        LOG_E("Failed to erase flash at 0x%08X\n", OTA_INFO_BASE_ADDR);
        ret = -1;
        goto __exit;
    }

    // Write firmware info to flash
    if (rt_flash_write(OTA_INFO_BASE_ADDR, (uint8_t *)temp_firmware_files,
                       data_size) != data_size)
    {
        LOG_E("Failed to write firmware info to flash\n");
        ret = -1;
        goto __exit;
    }

    LOG_I("Successfully parsed and stored %d firmware files info to flash\n",
          bin_num);

    struct firmware_info verify_firmware_files[MAX_FIRMWARE_FILES];
    if (rt_flash_read(OTA_INFO_BASE_ADDR, (uint8_t *)verify_firmware_files,
                      data_size) == data_size)
    {
        for (int i = 0; i < bin_num; i++)
        {
            LOG_I("Verified firmware file %d: %s\n", i,
                  verify_firmware_files[i].name);
        }
    }

    // 3. Processing actual firmware data (writing to Flash while downloading)
    LOG_I("Starting to process firmware data...\n");
    for (int i = 0; i < bin_num; i++)
    {
        struct firmware_info *fw_info = &temp_firmware_files[i];
        uint32_t remaining_length = fw_info->compressed_len;
        uint32_t data_written = 0;

        LOG_I("Processing firmware %s, size %d bytes\n", fw_info->name,
              remaining_length);

        //Send the name of the firmware that is being updated.
        dfu_pan_ui_update_message(UI_MSG_UPDATE_FILES, fw_info->name);

        // Check whether the firmware size is reasonable
        if (remaining_length == 0)
        {
            LOG_W("Warning: Firmware %s has zero length\n", fw_info->name);
            continue;
        }

        // Erase the target Flash area
        LOG_I("Erasing flash region for %s at 0x%08X, size 0x%08X\n",
              fw_info->name, fw_info->addr, fw_info->region_size);

        int erase_alignment = rt_flash_get_erase_alignment(fw_info->addr);
        int aligned_size =
            ((fw_info->region_size + erase_alignment - 1) / erase_alignment) *
            erase_alignment;

        if (rt_flash_erase(fw_info->addr, aligned_size) != RT_EOK)
        {
            LOG_E("Failed to erase flash for %s at 0x%08X\n", fw_info->name,
                  fw_info->addr);
            ret = -1;
            goto __exit;
        }

        // Stream processing firmware data
        // Read data in chunks and write to flash immediately to avoid memory overflow
        while (remaining_length > 0)
        {
            // Calculate chunk size, not exceeding buffer size
            int chunk_size = (remaining_length > OTA_RECV_BUFF_SIZE)
                                 ? OTA_RECV_BUFF_SIZE
                                 : remaining_length;
            int bytes_read = webclient_read(session, buffer, chunk_size);

            if (bytes_read <= 0)
            {
                LOG_E("Failed to read firmware data for %s\n", fw_info->name);
                ret = -1;
                goto __exit;
            }

            // Write data to flash immediately after reading
            if (rt_flash_write(fw_info->addr + data_written, (uint8_t *)buffer,
                               bytes_read) != bytes_read)
            {
                LOG_E("Failed to write firmware data to flash for %s\n",
                      fw_info->name);
                ret = -1;
                goto __exit;
            }

            remaining_length -= bytes_read;
            data_written += bytes_read;

            // Display progress
            if (fw_info->compressed_len > 0)
            {
                int progress =
                    ((fw_info->compressed_len - remaining_length) * 100) /
                    fw_info->compressed_len;

                int progress_level = progress / 5;
                int last_progress_level = last_sent_progress[i] / 5;

                if (progress_level != last_progress_level || progress == 100)
                {
                    LOG_I("Writing %s: %d%% (%d/%d bytes)\n", fw_info->name,
                          progress, fw_info->compressed_len - remaining_length,
                          fw_info->compressed_len);

                    char progress_str[8];
                    snprintf(progress_str, sizeof(progress_str), "%d",
                             progress);
                    // Update progress         
                    dfu_pan_ui_update_message(UI_MSG_UPDATE_PROGRESS, progress_str);

                    last_sent_progress[i] = progress;
                }

                else if (remaining_length == 0 && last_sent_progress[i] != 100)
                {
                    LOG_I("Writing %s: 100%% (%d/%d bytes)\n", fw_info->name,
                          fw_info->compressed_len, fw_info->compressed_len);

                    dfu_pan_ui_update_message(UI_MSG_UPDATE_PROGRESS, "100");
                    last_sent_progress[i] = 100;
                }
            }
        }

        LOG_I("Successfully wrote firmware %s to flash, %d bytes\n",
              fw_info->name, data_written);

        LOG_I("Verifying CRC for %s...\n", fw_info->name);
        uint8_t *verify_buffer = rt_malloc(OTA_RECV_BUFF_SIZE);
        if (verify_buffer)
        {

            uint32_t calculated_crc = 0xffffffff;

            uint32_t verify_remaining = fw_info->compressed_len;
            uint32_t verify_offset = 0;

            // Verify data integrity by calculating CRC
            while (verify_remaining > 0)
            {
                int verify_chunk = (verify_remaining > OTA_RECV_BUFF_SIZE)
                                       ? OTA_RECV_BUFF_SIZE
                                       : verify_remaining;

                if (rt_flash_read(fw_info->addr + verify_offset, verify_buffer,
                                  verify_chunk) != verify_chunk)
                {
                    LOG_E("Failed to read flash for CRC verification\n");
                    rt_free(verify_buffer);
                    ret = -1;
                    goto __exit;
                }

                calculated_crc = calculate_crc32(verify_buffer, verify_chunk,
                                                 calculated_crc);

                verify_remaining -= verify_chunk;
                verify_offset += verify_chunk;
            }

            if (calculated_crc != fw_info->data_crc)
            {
                LOG_E("CRC verification failed for %s. Expected: 0x%08X, Got: "
                      "0x%08X\n",
                      fw_info->name, fw_info->data_crc, calculated_crc);
                rt_free(verify_buffer);
                ret = -1;
                goto __exit;
            }

            LOG_I("CRC verification passed for %s\n", fw_info->name);
            rt_free(verify_buffer);
        }
        else
        {
            LOG_W("No memory for CRC verification buffer, skipping "
                  "verification\n");
        }
    }

__exit:
    if (session)
    {
        LOCK_TCPIP_CORE();
        webclient_close(session);
        UNLOCK_TCPIP_CORE();
    }

    if (buffer)
    {
        rt_free(buffer);
    }

    LOG_I("dfu_pan_parse_package_from_url finished with result: %d\n", ret);
    return ret;
}

int dfu_pan_get_version_info(int index, struct version_info *version_info)
{
    if (index < 0 || index >= MAX_VERSION_COUNT || version_info == NULL) {
        return -1;
    }
    
    uint32_t addr = VERSION_INFO_BASE_ADDR_FROM_MACRO + index * sizeof(struct version_info);
    int result = rt_flash_read(addr, (uint8_t *)version_info, sizeof(struct version_info));
    
    if (result != sizeof(struct version_info)) {
        LOG_E("Failed to read version info from flash at 0x%08X, result: %d", addr, result);
        return -1;
    }
    
    return 0;
}

int dfu_pan_get_firmware_info(int index, struct firmware_info *firmware_info)
{
    if (index < 0 || index >= MAX_FIRMWARE_FILES || firmware_info == NULL) {
        return -1;
    }
    
    uint32_t addr = FIRMWARE_INFO_BASE_ADDR + index * sizeof(struct firmware_info);
    int result = rt_flash_read(addr, (uint8_t *)firmware_info, sizeof(struct firmware_info));
    
    if (result != sizeof(struct firmware_info)) {
        LOG_E("Failed to read firmware info from flash at 0x%08X, result: %d", addr, result);
        return -1;
    }
    
    return 0;
}

rt_err_t dfu_pan_create_worker_thread(int version_index)
{

    rt_thread_t worker_thread = rt_thread_create(
        "ota_worker", dfu_pan_worker_thread_entry,
        (void *)(uintptr_t)version_index, OTA_WORKER_THREAD_STACK_SIZE, 25, 10);

    if (worker_thread != RT_NULL)
    {
        return rt_thread_startup(worker_thread);
    }
    return -RT_ERROR;
}

void dfu_pan_print_files(void)
{
    LOG_I("----------------OTA version_list Files Status:*******\n");
    LOG_I("==========================\n");
    LOG_I("OTA version_list Files Address: 0x%08X\n", VERSION_INFO_BASE_ADDR_FROM_MACRO);
    
    for (int i = 0; i < MAX_VERSION_COUNT; i++)
    {
        struct version_info version_info;
        if (dfu_pan_get_version_info(i, &version_info) == 0) {
            LOG_I("File %d:\n", i);
            LOG_I("  Filename: %s\n", version_info.name);
            LOG_I("  URL: \n");
            rt_kputs(version_info.zippath);
            LOG_I("-----");
            LOG_I("  Needs Update: %s\n", version_info.needs_update ? "1" : "0");
            LOG_I("  ------------------------\n");
        } else {
            LOG_E("Failed to read version info at index %d\n", i);
        }
    }
}

void dfu_pan_clear_update_flags(void)
{
    LOG_I("Clearing all update flags...\n");
    struct version_info temp_version_files[MAX_VERSION_COUNT];
    
    // Initialize the entire array to 0 and clear all version information.
    memset(temp_version_files, 0, sizeof(temp_version_files));

    int erase_alignment = rt_flash_get_erase_alignment(VERSION_INFO_BASE_ADDR_FROM_MACRO);
    int aligned_size = ((sizeof(struct version_info) * MAX_VERSION_COUNT +
                         erase_alignment - 1) /
                        erase_alignment) *
                       erase_alignment;
    rt_flash_erase(VERSION_INFO_BASE_ADDR_FROM_MACRO, aligned_size);
    rt_flash_write(VERSION_INFO_BASE_ADDR_FROM_MACRO, (uint8_t *)temp_version_files,
                   sizeof(struct version_info) * MAX_VERSION_COUNT);

    LOG_I("Update flags cleared successfully.\n");
}


void dfu_pan_test_update_flags(void)
{
    LOG_I("=== OTA Update Flags Test ===\n");
    
    // 1. 打印初始状态
    LOG_I("1. Initial state:\n");
    for (int i = 0; i < MAX_VERSION_COUNT; i++) {
        struct version_info temp_version;
        if (dfu_pan_get_version_info(i, &temp_version) == 0) {
            uint32_t needs_update_addr = VERSION_INFO_BASE_ADDR_FROM_MACRO + 
                                        i * sizeof(struct version_info) + 
                                        offsetof(struct version_info, needs_update);
            LOG_I("  Entry %d: needs_update=%d (addr: 0x%08x)", 
                  i, temp_version.needs_update, needs_update_addr);
        } else {
            uint32_t needs_update_addr = VERSION_INFO_BASE_ADDR_FROM_MACRO + 
                                        i * sizeof(struct version_info) + 
                                        offsetof(struct version_info, needs_update);
            LOG_I("  Entry %d: read failed (addr: 0x%08x)", i, needs_update_addr);
        }
    }
    
    // 2. 设置所有标志位为1
    LOG_I("2. Setting all flags to 1:\n");
    struct version_info temp_version_files[MAX_VERSION_COUNT];
    
    // 直接初始化所有条目，只设置需要的字段
    for (int i = 0; i < MAX_VERSION_COUNT; i++) {
        memset(&temp_version_files[i], 0, sizeof(struct version_info));
        temp_version_files[i].needs_update = 1;
        LOG_I("  Setting entry %d needs_update = 1", i);
    }
    
    // 写入flash
    int erase_alignment = rt_flash_get_erase_alignment(VERSION_INFO_BASE_ADDR_FROM_MACRO);
    int aligned_size = ((sizeof(struct version_info) * MAX_VERSION_COUNT +
                         erase_alignment - 1) /
                        erase_alignment) *
                       erase_alignment;
                       
    if (rt_flash_erase(VERSION_INFO_BASE_ADDR_FROM_MACRO, aligned_size) != RT_EOK) {
        LOG_E("Failed to erase flash at 0x%08X", VERSION_INFO_BASE_ADDR_FROM_MACRO);
        return;
    }
    
    int write_result = rt_flash_write(VERSION_INFO_BASE_ADDR_FROM_MACRO, 
                                      (uint8_t *)temp_version_files,
                                      sizeof(struct version_info) * MAX_VERSION_COUNT);
                                      
    if (write_result != sizeof(struct version_info) * MAX_VERSION_COUNT) {
        LOG_E("Failed to write to flash. Expected: %d, Actual: %d", 
              sizeof(struct version_info) * MAX_VERSION_COUNT, write_result);
        return;
    }
    LOG_I("All flags set to 1 successfully.\n");
    
    // 3. 验证所有标志位是否为1
    LOG_I("3. Verifying all flags are set to 1:\n");
    for (int i = 0; i < MAX_VERSION_COUNT; i++) {
        struct version_info temp_version;
        if (dfu_pan_get_version_info(i, &temp_version) == 0) {
            uint32_t needs_update_addr = VERSION_INFO_BASE_ADDR_FROM_MACRO + 
                                        i * sizeof(struct version_info) + 
                                        offsetof(struct version_info, needs_update);
            LOG_I("  Entry %d: needs_update=%d (addr: 0x%08x) %s", 
                  i, temp_version.needs_update, needs_update_addr,
                  (temp_version.needs_update == 1) ? "OK" : "ERROR");
        } else {
            uint32_t needs_update_addr = VERSION_INFO_BASE_ADDR_FROM_MACRO + 
                                        i * sizeof(struct version_info) + 
                                        offsetof(struct version_info, needs_update);
            LOG_E("  Entry %d: read failed (addr: 0x%08x)", i, needs_update_addr);
        }
    }
    
    // 4. 清除所有标志位为0
    LOG_I("4. Clearing all flags to 0:\n");
    // 重新初始化数组
    for (int i = 0; i < MAX_VERSION_COUNT; i++) {
        memset(&temp_version_files[i], 0, sizeof(struct version_info));
        temp_version_files[i].needs_update = 0;
        LOG_I("  Setting entry %d needs_update = 0", i);
    }
    
    // 写入flash
    if (rt_flash_erase(VERSION_INFO_BASE_ADDR_FROM_MACRO, aligned_size) != RT_EOK) {
        LOG_E("Failed to erase flash at 0x%08X", VERSION_INFO_BASE_ADDR_FROM_MACRO);
        return;
    }
    
    write_result = rt_flash_write(VERSION_INFO_BASE_ADDR_FROM_MACRO, 
                                  (uint8_t *)temp_version_files,
                                  sizeof(struct version_info) * MAX_VERSION_COUNT);
                                  
    if (write_result != sizeof(struct version_info) * MAX_VERSION_COUNT) {
        LOG_E("Failed to write to flash. Expected: %d, Actual: %d", 
              sizeof(struct version_info) * MAX_VERSION_COUNT, write_result);
        return;
    }
    LOG_I("All flags cleared to 0 successfully.\n");
    
    // 5. 验证所有标志位是否为0
    LOG_I("5. Verifying all flags are cleared to 0:\n");
    for (int i = 0; i < MAX_VERSION_COUNT; i++) {
        struct version_info temp_version;
        if (dfu_pan_get_version_info(i, &temp_version) == 0) {
            uint32_t needs_update_addr = VERSION_INFO_BASE_ADDR_FROM_MACRO + 
                                        i * sizeof(struct version_info) + 
                                        offsetof(struct version_info, needs_update);
            LOG_I("  Entry %d: needs_update=%d (addr: 0x%08x) %s", 
                  i, temp_version.needs_update, needs_update_addr,
                  (temp_version.needs_update == 0) ? "OK" : "ERROR");
        } else {
            uint32_t needs_update_addr = VERSION_INFO_BASE_ADDR_FROM_MACRO + 
                                        i * sizeof(struct version_info) + 
                                        offsetof(struct version_info, needs_update);
            LOG_E("  Entry %d: read failed (addr: 0x%08x)", i, needs_update_addr);
        }
    }
    // Clear all update information
    dfu_pan_clear_update_flags();
    LOG_I("=== OTA Update Flags Test Complete ===\n");
}