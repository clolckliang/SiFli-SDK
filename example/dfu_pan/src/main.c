// OTA program entry: Download, Install

#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include <stdio.h>
#include <string.h>
#include "register.h"
#include "bt_pan_ota.h"
// Add the relevant Bluetooth header files.
#include "bts2_app_inc.h"
#include "bt_connection_manager.h"
#include "ble_connection_manager.h"
#include "bf0_sibles_internal.h"
#define LOG_TAG "ota_install"
#include "log.h"
#include <lwip/sys.h>
#include "lwip/tcpip.h"
#include "lwip/dns.h"
#include <netdb.h>

#if defined(RT_USING_DFS_MNTTABLE)
    #include "dfs_posix.h"
    #include "flash_map.h"
const struct dfs_mount_tbl mount_table[] = {FS_MOUNT_TABLE};
#endif

#define CUSTOM_OTA_MODE_REBOOT_TO_OTA 1
#define CUSTOM_OTA_MODE_NONE 0

// Bluetooth PAN-related Definitions
#define BT_APP_READY 1
#define BT_APP_CONNECT_PAN 2
#define OTA_BT_APP_CONNECT_PAN_SUCCESS 3
#define OTA_AUTO_UPDATE_CMD 4
#define PAN_TIMER_MS 3000

static rt_mailbox_t g_bt_app_mb;
BOOL dfu_pan_connected = FALSE;
#define BLUETOOTH_NAME "sifli-pan"

// UI-related parameters
#include "dfu_pan_ui.h"
#define DFU_PAN_UI_THREAD_STACK_SIZE (8192)
static struct rt_thread dfu_pan_ui_thread;
static uint8_t dfu_pan_ui_thread_stack[8192];
static rt_timer_t auto_update_check_timer = RT_NULL;



static int test_dns_resolution(const char *hostname)
{
    ip_addr_t addr = {0};
    err_t err;

    LOG_I("Testing DNS resolution for: %s", hostname);

    // Using the DNS resolution function of lwIP
    err = dns_gethostbyname(hostname, &addr, NULL, NULL);
    if (err != ERR_OK && err != ERR_INPROGRESS)
    {
        LOG_E("DNS resolution failed for %s, error: %d", hostname, err);
        return -1;
    }

    LOG_I("DNS resolution successful for %s", hostname);
    return 0;
}

static int dns_check_try(void)
{
    // Keep attempting DNS resolution until it succeeds or reaches the maximum
    // retry count.
    int dns_retry_count = 0;
    const int max_dns_retries = 10; // Maximum retry attempts: 10 times
    int dns_result = -1;

    while (dns_retry_count < max_dns_retries)
    {
        dns_result = test_dns_resolution("ota.sifli.com");
        if (dns_result == 0)
        {
            LOG_I("DNS resolution successful after %d attempts",
                  dns_retry_count + 1);
            break;
        }
        else
        {
            LOG_W("DNS resolution failed, attempt %d/%d", dns_retry_count + 1,
                  max_dns_retries);
            dns_retry_count++;
            if (dns_retry_count < max_dns_retries)
            {
                rt_thread_mdelay(1000); // Wait for 1 second and then try again.
            }
        }
    }
    return dns_result;
}

void bt_app_connect_pan_timeout_handle(void *parameter)
{
    LOG_I("bt_app_connect_pan_timeout_handle %x, %d", g_bt_app_mb,
          g_bt_app_env_ota.bt_connected);
    if ((g_bt_app_mb != NULL) && (g_bt_app_env_ota.bt_connected))
        rt_mb_send(g_bt_app_mb, BT_APP_CONNECT_PAN);
    return;
}

// Automatic update check timer callback function
static void auto_update_check_callback(void *parameter)
{
    LOG_I("Auto update check timer expired, checking connection status");

    // Check the network connection status
    if (dfu_pan_connected == TRUE)
    {
        rt_thread_mdelay(
            1000); // Wait for 1 second to ensure a stable connection
        rt_mb_send(g_bt_app_mb, OTA_AUTO_UPDATE_CMD); // Send to the main thread
                                                      // for automatic update.
    }
    else
    {
        LOG_W("Network connection lost within 1 second, skipping auto update");
    }
}
static int bt_app_interface_event_handle(uint16_t type, uint16_t event_id,
                                         uint8_t *data, uint16_t data_len)
{

    LOG_I("bt_app_interface_event_handle_type: %d\n", type);
    if (type == BT_NOTIFY_COMMON)
    {
        int pan_conn = 0;

        switch (event_id)
        {
        case BT_NOTIFY_COMMON_BT_STACK_READY:
        {
            LOG_I("BT_NOTIFY_COMMON_BT_STACK_READY\n");
            rt_mb_send(g_bt_app_mb, BT_APP_READY);
        }
        break;
        case BT_NOTIFY_COMMON_ACL_CONNECTED:
        {
            dfu_pan_ui_update_message(UI_MSG_UPDATE_BLE,
                                      UI_MSG_DATA_BLE_CONNECTED);
            LOG_I("BT_NOTIFY_COMMON_ACL_CONNECTED\n");
        }
        break;
        case BT_NOTIFY_COMMON_ACL_DISCONNECTED:
        {
            dfu_pan_ui_update_message(UI_MSG_UPDATE_BLE,
                                      UI_MSG_DATA_BLE_DISCONNECTED);

            bt_notify_device_base_info_t *info =
                (bt_notify_device_base_info_t *)data;
            LOG_I("disconnected(0x%.2x:%.2x:%.2x:%.2x:%.2x:%.2x) res %d",
                  info->mac.addr[5], info->mac.addr[4], info->mac.addr[3],
                  info->mac.addr[2], info->mac.addr[1], info->mac.addr[0],
                  info->res);
            g_bt_app_env_ota.bt_connected = FALSE;
            //  memset(&g_bt_app_env_ota.bd_addr, 0xFF,
            //  sizeof(g_bt_app_env_ota.bd_addr));
            if (info->res == BT_NOTIFY_COMMON_SCO_DISCONNECTED)
            {

                LOG_I("Phone actively disconnected, prepare to enter sleep "
                      "mode after 30 seconds");
            }
            else
            {
                LOG_I("Abnormal disconnection, start reconnect attempts");
            }

            if (g_bt_app_env_ota.pan_connect_timer)
                rt_timer_stop(g_bt_app_env_ota.pan_connect_timer);
        }
        break;
        case BT_NOTIFY_COMMON_ENCRYPTION:
        {
            bt_notify_device_mac_t *mac = (bt_notify_device_mac_t *)data;
            LOG_I("Encryption competed");
            g_bt_app_env_ota.bd_addr = *mac;
            pan_conn = 1;
        }
        break;
        case BT_NOTIFY_COMMON_PAIR_IND:
        {
            bt_notify_device_base_info_t *info =
                (bt_notify_device_base_info_t *)data;
            LOG_I("Pairing completed %d", info->res);
            if (info->res == BTS2_SUCC)
            {
                g_bt_app_env_ota.bd_addr = info->mac;
                pan_conn = 1;
            }
        }
        break;
        case BT_NOTIFY_COMMON_KEY_MISSING:
        {
            bt_notify_device_base_info_t *info =
                (bt_notify_device_base_info_t *)data;
            LOG_I("Key missing %d", info->res);
            memset(&g_bt_app_env_ota.bd_addr, 0xFF,
                   sizeof(g_bt_app_env_ota.bd_addr));
            bt_cm_delete_bonded_devs_and_linkkey(info->mac.addr);
        }
        break;
        default:
            break;
        }

        if (pan_conn)
        {
            LOG_I("bd addr 0x%.2x:%.2x:%.2x:%.2x:%.2x:%.2x\n",
                  g_bt_app_env_ota.bd_addr.addr[5],
                  g_bt_app_env_ota.bd_addr.addr[4],
                  g_bt_app_env_ota.bd_addr.addr[3],
                  g_bt_app_env_ota.bd_addr.addr[2],
                  g_bt_app_env_ota.bd_addr.addr[1],
                  g_bt_app_env_ota.bd_addr.addr[0]);
            g_bt_app_env_ota.bt_connected = TRUE;
            // Trigger PAN connection after PAN_TIMER_MS period to avoid SDP
            // confliction.
            if (!g_bt_app_env_ota.pan_connect_timer)
                g_bt_app_env_ota.pan_connect_timer = rt_timer_create(
                    "connect_pan", bt_app_connect_pan_timeout_handle,
                    (void *)&g_bt_app_env_ota,
                    rt_tick_from_millisecond(PAN_TIMER_MS),
                    RT_TIMER_FLAG_SOFT_TIMER);
            else
                rt_timer_stop(g_bt_app_env_ota.pan_connect_timer);
            rt_timer_start(g_bt_app_env_ota.pan_connect_timer);
        }
    }
    else if (type == BT_NOTIFY_PAN)
    {
        switch (event_id)
        {
        case BT_NOTIFY_PAN_PROFILE_CONNECTED:
        {
            LOG_I("pan connect successed \n");
            if ((g_bt_app_env_ota.pan_connect_timer))
            {
                rt_timer_stop(g_bt_app_env_ota.pan_connect_timer);
            }
            rt_mb_send(g_bt_app_mb, OTA_BT_APP_CONNECT_PAN_SUCCESS);
            dfu_pan_ui_update_message(UI_MSG_UPDATE_NET,
                                      UI_MSG_DATA_NET_CONNECTED);
            dfu_pan_connected = TRUE;
        }
        break;
        case BT_NOTIFY_PAN_PROFILE_DISCONNECTED:
        {

            LOG_I("pan disconnect with remote device\n");
            dfu_pan_connected = FALSE;
            dfu_pan_ui_update_message(UI_MSG_UPDATE_NET,
                                      UI_MSG_DATA_NET_DISCONNECTED);
        }
        break;
        default:
            break;
        }
    }
    else if (type == BT_NOTIFY_HID)
    {
        switch (event_id)
        {
        case BT_NOTIFY_HID_PROFILE_CONNECTED:
        {
            LOG_I("HID connected\n");
            if (!dfu_pan_connected)
            {
                if (g_bt_app_env_ota.pan_connect_timer)
                {
                    rt_timer_stop(g_bt_app_env_ota.pan_connect_timer);
                }
                bt_interface_conn_ext((char *)&g_bt_app_env_ota.bd_addr,
                                      BT_PROFILE_PAN);
            }
        }
        break;
        case BT_NOTIFY_HID_PROFILE_DISCONNECTED:
        {
            LOG_I("HID disconnected\n");
        }
        break;
        default:
            break;
        }
    }

    return 0;
}

int main(void)
{

    rt_err_t result = rt_thread_init(
        &dfu_pan_ui_thread, "dfu_pan_ui", dfu_pan_ui_task, NULL,
        &dfu_pan_ui_thread_stack[0], DFU_PAN_UI_THREAD_STACK_SIZE, 30, 10);
    if (result == RT_EOK)
    {
        rt_kprintf("xiaozhi UI thread init success\n");
        rt_thread_startup(&dfu_pan_ui_thread);
    }
    else
    {
        rt_kprintf("Failed to init xiaozhi UI thread\n");
    }

    g_bt_app_mb = rt_mb_create("bt_app", 8, RT_IPC_FLAG_FIFO);

#ifdef BSP_BT_CONNECTION_MANAGER
    bt_cm_set_profile_target(BT_CM_HID, BT_LINK_PHONE, 1);
#endif // BSP_BT_CONNECTION_MANAGER

    bt_interface_status_t status =
        bt_interface_register_bt_event_notify_callback(
            bt_app_interface_event_handle);
    sifli_ble_enable();

    LOG_I("---sifli_ble_enable---\n");

    while (1)
    {

        uint32_t value;

        // handle pan connect event
        rt_mb_recv(g_bt_app_mb, (rt_uint32_t *)&value, RT_WAITING_FOREVER);

        if (value == BT_APP_CONNECT_PAN)
        {
            LOG_I("BT_APP_CONNECT_PAN\n");
            if (g_bt_app_env_ota.bt_connected)
            {
                bt_interface_conn_ext((char *)&g_bt_app_env_ota.bd_addr,
                                      BT_PROFILE_PAN);
            }
        }
        else if (value == BT_APP_READY)
        {
            LOG_I("BT/BLE stack and profile ready");

#ifdef BT_NAME_MAC_ENABLE
            LOG_I("BT_NAME_MAC_Local-name: %s", BT_NAME_MAC);
            char local_name[32];
            bd_addr_t addr;
            ble_get_public_address(&addr);
            sprintf(local_name, "%s-%02x:%02x:%02x:%02x:%02x:%02x",
                    BLUETOOTH_NAME, addr.addr[0], addr.addr[1], addr.addr[2],
                    addr.addr[3], addr.addr[4], addr.addr[5]);
#else
            const char *local_name = BLUETOOTH_NAME;
#endif

            LOG_I("------Set_local_name------: %s\n", local_name);

            bt_interface_set_local_name(strlen(local_name), (void *)local_name);
        }
        else if (value == OTA_BT_APP_CONNECT_PAN_SUCCESS)
        {

            LOG_I("Auto update enabled, starting 1-second delay timer for "
                  "connection stability check");

            // Create or restart the automatic update check timer
            if (auto_update_check_timer == RT_NULL)
            {
                auto_update_check_timer = rt_timer_create(
                    "auto_update_check", auto_update_check_callback, RT_NULL,
                    rt_tick_from_millisecond(1000),
                    RT_TIMER_FLAG_ONE_SHOT | RT_TIMER_FLAG_SOFT_TIMER);
            }
            else
            {
                rt_timer_stop(auto_update_check_timer);
            }

            // Start the timer
            if (auto_update_check_timer != RT_NULL)
            {
                rt_timer_start(auto_update_check_timer);
                LOG_I("Auto update check timer started");
            }

            LOG_I("------start_ota------\n");
        }
        else if (value == OTA_AUTO_UPDATE_CMD)
        {

            int dns_result = dns_check_try();
            if (dns_result != 0)
            {
                LOG_E("DNS resolution failed after multiple attempts, aborting "
                      "auto update");
                continue;
            }

            struct firmware_file_info firmware_files[MAX_FIRMWARE_FILES];
            int file_count = 0;

            for (int i = 0; i < MAX_FIRMWARE_FILES; i++)
            {
                if (dfu_pan_get_firmware_file_info(
                        i, &firmware_files[file_count]) == 0)
                {
                    // Check if it is a valid firmware file (with a non-empty
                    // name)
                    if (firmware_files[file_count].name[0] != '\0')
                    {
                        file_count++;
                    }
                }
            }

            if (file_count > 0)
            {
                // Download all firmware files
                int ret = dfu_pan_download_firmware(firmware_files, file_count);
                if (ret == 0)
                {
                    LOG_I("Firmware download initiated successfully");
                    dfu_pan_clear_files();
                    rt_kprintf("Restarting device...\n");
                    HAL_PMU_Reboot(); // Restart the device
                }
                else
                {
                    LOG_E("Firmware download initiation failed with error "
                          "code: %d",
                          ret);
                    dfu_pan_ui_update_message(UI_MSG_SHOW_FAILURE_POPUP,
                                              "连接网络后重试");
                }
            }
            else
            {
                LOG_E("No valid firmware files found for download");
            }
        }
    }

    return RT_EOK;
}

static void ota_cmd_start(int argc, char **argv)
{
    if (strcmp(argv[1], "del_bond") == 0)
    {
#ifdef BSP_BT_CONNECTION_MANAGER
        bt_cm_delete_bonded_devs();
        LOG_D("Delete bond");
#endif // BSP_BT_CONNECTION_MANAGER
    }
    // only valid after connection setup but phone didn't enable pernal hop
    else if (strcmp(argv[1], "conn_pan") == 0)
        bt_app_connect_pan_timeout_handle(NULL);
}
MSH_CMD_EXPORT(ota_cmd_start, Connect PAN to last paired device);

static void dfu_pan_finish_cmd(int argc, char **argv)
{
    dfu_pan_test_update_flags();
}
MSH_CMD_EXPORT(dfu_pan_finish_cmd, OTA finish verification command);

// Print file information
static void dfu_pan_print_files_cmd(int argc, char **argv)
{

    dfu_pan_print_files();
}
MSH_CMD_EXPORT(dfu_pan_print_files_cmd, Print files information);

// Set update flag

static void dfu_pan_set_update_flags_cmd(int argc, char **argv)
{

    dfu_pan_set_update_flags();
}
MSH_CMD_EXPORT(dfu_pan_set_update_flags_cmd, Set Update Flags);

// Delete file information
static void dfu_pan_clear_files_cmd(int argc, char **argv)
{

    dfu_pan_clear_files();
}
MSH_CMD_EXPORT(dfu_pan_clear_files_cmd, clear files information);

static void dfu_pan_download_firmware_cmd(int argc, char **argv)
{
    struct firmware_file_info firmware_files[MAX_FIRMWARE_FILES];
    int valid_files = 0;
    for (int i = 0; i < MAX_FIRMWARE_FILES; i++)
    {
        if (dfu_pan_get_firmware_file_info(i, &firmware_files[i]) == 0)
        {
            LOG_I("File %d:\n", i);
            LOG_I("  Name: %s\n", firmware_files[i].name);
            LOG_I("url: \n");
            rt_kputs(firmware_files[i].url);
            LOG_I("\n");
            LOG_I("  Address: 0x%08X\n", firmware_files[i].addr);
            LOG_I("  file Size: %d bytes\n", firmware_files[i].size);
            LOG_I("  CRC: 0x%08X\n", firmware_files[i].crc32);
            LOG_I("  Region Size: %d bytes\n", firmware_files[i].region_size);
            LOG_I("  file_id: %d\n", firmware_files[i].file_id);
            LOG_I("  needs_update: %d\n", firmware_files[i].needs_update);
            LOG_I("  magic: %d\n", firmware_files[i].magic);
            LOG_I("  ------------------------\n");
            valid_files++;
        }
        else
        {
            LOG_E("Failed to read firmware info at index %d\n", i);
            // Invalid item initialization
            memset(&firmware_files[i], 0, sizeof(struct firmware_file_info));
        }
    }
    int result = dfu_pan_download_firmware(firmware_files, 3);
    if (result == 0)
    {
        LOG_I("Firmware download initiated successfully");
        dfu_pan_clear_files();
    }
    else
    {
        LOG_E("Firmware download initiation failed with error code: %d",
              result);
    }
}
MSH_CMD_EXPORT(dfu_pan_download_firmware_cmd, clear files information);

// Testing DNS resolution
static void test_dns_resolution_cmd(int argc, char **argv)
{
    test_dns_resolution("ota.sifli.com");
}
MSH_CMD_EXPORT(test_dns_resolution_cmd, OTA finish verification command);
