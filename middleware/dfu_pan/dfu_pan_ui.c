#include "dfu_pan_ui.h"
#include "bt_pan_ota.h"
#include "littlevgl2rtt.h"
#include "lv_obj_pos.h"
#include "register.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
lv_obj_t *uic_Image1;

lv_obj_t *ui_emoji;
lv_obj_t *ui_ble;
lv_obj_t *ui_network;
lv_obj_t *ui_update;
lv_obj_t *ui_progress;
lv_obj_t *ui_prolabel;
lv_obj_t *ui_status;
lv_obj_t *Update_sign;

lv_obj_t *completion_popup = NULL;
lv_obj_t *version_list_container = NULL;
lv_obj_t *close_button = NULL;
lv_obj_t *close_button_label = NULL;

static lv_obj_t *confirm_popup = NULL;
static lv_obj_t *confirm_label = NULL;
static lv_obj_t *confirm_btn = NULL;
static lv_obj_t *cancel_btn = NULL;
static lv_obj_t *confirm_btn_label = NULL;
static lv_obj_t *cancel_btn_label = NULL;
static int selected_version_index = -1;
//Image resources
extern const lv_image_dsc_t ota_emoji; 
extern const lv_image_dsc_t ota_ble;   
extern const lv_image_dsc_t ota_ble_close;
extern const lv_image_dsc_t ota_network_icon_img;
extern const lv_image_dsc_t ota_network_icon_img_close; 
rt_mq_t ota_ui_msg_queue = RT_NULL;
static float g_scale = 1.0f;
static char g_confirm_text_buffer[128];
// Screen Reference value
#define BASE_WIDTH 390
#define BASE_HEIGHT 450

static float get_scale_factor(void)
{
    lv_disp_t *disp = lv_disp_get_default();
    lv_coord_t scr_width = lv_disp_get_hor_res(disp);
    lv_coord_t scr_height = lv_disp_get_ver_res(disp);

    float scale_x = (float)scr_width / BASE_WIDTH;
    float scale_y = (float)scr_height / BASE_HEIGHT;

    return (scale_x < scale_y) ? scale_x : scale_y;
}
static void ui_free(char *str)
{
    if (str)
    {
        rt_free(str);
    }
}
// Display the list of versions
static void show_version_list_popup(void)
{
    rt_kprintf("show_version_list_popup\n");
    // Display pop-up box
    if (completion_popup)
    {
        lv_obj_remove_flag(completion_popup, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(completion_popup); 
    }
}

// Hidden Version List Popup Box
static void hide_version_list_popup(void)
{
    rt_kprintf("hide_version_list_popup\n");

    if (completion_popup)
    {
        lv_obj_add_flag(completion_popup, LV_OBJ_FLAG_HIDDEN);
    }
}

// Button click event callback function - Displays version list
static void update_button_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED)
    {
        rt_kprintf("OPEN VERSION LIST POPUP\n");
        show_version_list_popup(); // Open the version box
    }
}

// Close the version box
static void close_button_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED)
    {
        rt_kprintf("CLOSE VERSION LIST POPUP\n");
        hide_version_list_popup();
    }
}

// Version button event callback function
static void version_button_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED)
    {
        lv_obj_t *btn = lv_event_get_target(e);
        // Obtain the index from the user data
        int version_index = (int)(uintptr_t)lv_obj_get_user_data(btn);

        // 
        if (version_index >= 0 && version_index < MAX_VERSION_COUNT) {
            // Verify the validity of the index
            struct version_info temp_version;
            if (dfu_pan_get_version_info(version_index, &temp_version) == 0 && 
                temp_version.name[0] != '\0') {
                selected_version_index = version_index;
            }
        }

        // Only send the confirmation message when an effective version is found.
        if (selected_version_index >= 0)
        {
            dfu_pan_ui_update_message(UI_MSG_SHOW_VERSION_CONFIRM, NULL);
        }
    }
}

// Version confirmation button event callback function
static void confirm_btn_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED)
    {
        // Obtain the text of the confirmation button to determine the operation type
        const char *btn_text = lv_label_get_text(confirm_btn_label);

        if (strcmp(btn_text, "确认") == 0)
        {
            dfu_pan_ui_update_message(UI_MSG_START_OTA_UPDATE, NULL);
        }
        else if (strcmp(btn_text, "重试") == 0)
        {
            dfu_pan_ui_update_message(UI_MSG_START_OTA_UPDATE, NULL);
        }
        else if (strcmp(btn_text, "重启") == 0)
        {
            lv_obj_add_flag(confirm_popup, LV_OBJ_FLAG_HIDDEN);
            rt_kprintf("Restarting device...\n");
            HAL_PMU_Reboot();// Restart the device
        }
    }
}

// Version cancellation button event callback function
static void cancel_btn_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED)
    {
        lv_obj_add_flag(confirm_popup, LV_OBJ_FLAG_HIDDEN);
        if (completion_popup)
        {
            lv_obj_remove_flag(completion_popup, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

// Message queue update
void dfu_pan_ui_update_message(ui_msg_type_t type, char *string)
{
    if (ota_ui_msg_queue != RT_NULL)
    {
        ui_msg_t *msg = (ui_msg_t *)rt_malloc(sizeof(ui_msg_t));
        if (msg != RT_NULL)
        {
            msg->type = type;
            msg->data = rt_strdup(string);
            rt_err_t result =
                rt_mq_send(ota_ui_msg_queue, &msg, sizeof(ui_msg_t *));
            if (result != RT_EOK)
            {
                rt_kprintf("Failed to send UI message: type=%d, error=%d\n",
                           type, result);
                rt_free(msg->data);
                rt_free(msg);
            }
            else
            {
                rt_kprintf("send success: %d\n", result);
            }
        }
        else
        {
            rt_kprintf("Failed to allocate memory for UI message: type=%d\n",
                       type);
        }
    }
    else
    {
        rt_kprintf("UI message queue is NULL, message not sent: type=%d\n",
                   type);
    }
}

extern const lv_font_t ota_font;
rt_err_t ota_ui_obj_init(float scale)
{
    static lv_style_t style;

    lv_style_init(&style);
    if (scale <= 0.5f)
    {
        lv_style_set_text_font(&style, lv_font_ubuntu_12);
    }
    else if (scale <= 0.7f)
    {
        lv_style_set_text_font(&style, lv_font_ubuntu_16);
    }
    else if (scale <= 1.0f)
    {
        lv_style_set_text_font(&style, lv_font_ubuntu_24);
    }
    else if (scale <= 1.3f)
    {
        lv_style_set_text_font(&style, lv_font_ubuntu_36);
    }
    else if (scale <= 1.7f)
    {
        lv_style_set_text_font(&style, lv_font_ubuntu_56);
    }
    else
    {
        lv_style_set_text_font(&style, lv_font_ubuntu_72);
    }
    lv_style_set_text_align(&style, LV_TEXT_ALIGN_CENTER);

    LV_IMAGE_DECLARE(ota_emoji);
    LV_IMAGE_DECLARE(ota_ble_close);
    LV_IMAGE_DECLARE(ota_ble);
    LV_IMAGE_DECLARE(ota_network_icon_img);
    LV_IMAGE_DECLARE(ota_network_icon_img_close);

    lv_obj_t *ui_Screen1 = lv_obj_create(lv_screen_active());
    lv_obj_remove_flag(ui_Screen1, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(ui_Screen1, 0, 0);
    lv_obj_set_size(ui_Screen1, LV_PCT(100), LV_PCT(100));

    ui_emoji = lv_image_create(ui_Screen1);
    lv_image_set_src(ui_emoji, &ota_emoji);
    lv_obj_set_width(ui_emoji, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_emoji, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_emoji, (int)(-129 * scale));
    lv_obj_set_y(ui_emoji, (int)(-160 * scale));
    lv_obj_set_align(ui_emoji, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_emoji, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(ui_emoji, LV_OBJ_FLAG_SCROLLABLE);

    ui_ble = lv_image_create(ui_Screen1);
    lv_image_set_src(ui_ble, &ota_ble_close);
    lv_obj_set_width(ui_ble, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_ble, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_ble, (int)(-129 * scale));
    lv_obj_set_y(ui_ble, (int)(-23 * scale));
    lv_obj_set_align(ui_ble, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_ble, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(ui_ble, LV_OBJ_FLAG_SCROLLABLE);

    ui_network = lv_image_create(ui_Screen1);
    lv_image_set_src(ui_network, &ota_network_icon_img_close);
    lv_obj_set_width(ui_network, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_network, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_network, (int)(-129 * scale));
    lv_obj_set_y(ui_network, (int)(114 * scale));
    lv_obj_set_align(ui_network, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_network, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(ui_network, LV_OBJ_FLAG_SCROLLABLE);

    ui_update = lv_button_create(ui_Screen1);
    lv_obj_set_width(ui_update, (int)(143 * scale));
    lv_obj_set_height(ui_update, (int)(34 * scale));
    lv_obj_set_x(ui_update, (int)(62 * scale));
    lv_obj_set_y(ui_update, (int)(109 * scale));
    lv_obj_set_align(ui_update, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_update, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_remove_flag(ui_update, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_add_event_cb(ui_update, update_button_event_cb, LV_EVENT_CLICKED,
                        NULL);

    Update_sign = lv_label_create(ui_update);
    lv_obj_set_width(Update_sign, LV_SIZE_CONTENT);
    lv_obj_set_height(Update_sign, LV_SIZE_CONTENT);
    lv_obj_align(Update_sign, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_style(Update_sign, &style, 0);
    lv_label_set_text(Update_sign, "版本列表");
    lv_obj_set_style_text_font(Update_sign, &ota_font, 0);

    ui_progress = lv_arc_create(ui_Screen1);
    lv_obj_set_size(ui_progress, (int)(200 * scale), (int)(200 * scale));
    lv_obj_set_x(ui_progress, (int)(63 * scale));
    lv_obj_set_y(ui_progress, (int)(-75 * scale));
    lv_obj_set_align(ui_progress, LV_ALIGN_CENTER);
    lv_arc_set_rotation(ui_progress, 270);
    lv_arc_set_bg_angles(ui_progress, 0, 360);
    lv_arc_set_value(ui_progress, 100);
    lv_obj_remove_style(ui_progress, NULL, LV_PART_KNOB);

    lv_obj_set_style_arc_color(ui_progress, lv_color_hex(0x333333),
                               LV_PART_MAIN);
    lv_obj_set_style_arc_width(ui_progress, (int)(25 * scale), LV_PART_MAIN);

    lv_obj_set_style_arc_color(ui_progress, lv_color_hex(0x00a0ff),
                               LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(ui_progress, (int)(25 * scale),
                               LV_PART_INDICATOR);

    lv_obj_remove_flag(ui_progress, LV_OBJ_FLAG_CLICKABLE);

    ui_prolabel = lv_label_create(ui_Screen1);
    lv_obj_set_width(ui_prolabel, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_prolabel, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_prolabel, (int)(66 * scale));
    lv_obj_set_y(ui_prolabel, (int)(-74 * scale));
    lv_obj_set_align(ui_prolabel, LV_ALIGN_CENTER);
    lv_obj_add_style(ui_prolabel, &style, 0);
    lv_label_set_text(ui_prolabel, "100%");
    lv_obj_set_style_text_font(Update_sign, &ota_font, 0);

    ui_status = lv_label_create(ui_Screen1);
    lv_obj_set_width(ui_status, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_status, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_status, (int)(64 * scale));
    lv_obj_set_y(ui_status, (int)(49 * scale));
    lv_obj_set_align(ui_status, LV_ALIGN_CENTER);
    lv_obj_add_style(ui_status, &style, 0);
    lv_label_set_text(ui_status, "下载中...");
    lv_obj_set_style_text_font(ui_status, &ota_font, 0);

    completion_popup = lv_obj_create(lv_screen_active());
    lv_obj_set_size(completion_popup, (int)(300 * scale), (int)(200 * scale));
    lv_obj_center(completion_popup);
    lv_obj_set_style_bg_color(completion_popup, lv_color_hex(0x202020), 0);
    lv_obj_set_style_border_color(completion_popup, lv_color_hex(0x00a0ff), 0);
    lv_obj_set_style_border_width(completion_popup, (int)(2 * scale), 0);
    lv_obj_set_style_radius(completion_popup, (int)(10 * scale), 0);
    lv_obj_add_flag(completion_popup, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_scrollbar_mode(completion_popup, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(completion_popup, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *close_btn = lv_button_create(completion_popup);
    lv_obj_set_size(close_btn, (int)(35 * scale), (int)(35 * scale));
    lv_obj_align(close_btn, LV_ALIGN_TOP_RIGHT, (int)(35 * scale),
                 (int)(-35 * scale));
    lv_obj_set_style_radius(close_btn, (int)(15 * scale), 0);
    lv_obj_set_style_bg_color(close_btn, lv_color_hex(0xFF0000), 0);
    lv_obj_set_style_bg_color(close_btn, lv_color_hex(0xCC0000),
                              LV_STATE_PRESSED);
    lv_obj_set_style_border_width(close_btn, 0, 0);
    lv_obj_set_style_border_width(close_btn, (int)(2 * scale), 0);
    lv_obj_set_style_border_color(close_btn, lv_color_hex(0xFFFF00), 0);

    lv_obj_t *close_label = lv_label_create(close_btn);
    lv_label_set_text(close_label, "X");
    lv_obj_center(close_label);
    lv_obj_set_style_text_color(close_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(close_label, &ota_font, 0);

    lv_obj_add_event_cb(close_btn, close_button_event_cb, LV_EVENT_CLICKED,
                        NULL);

    version_list_container = lv_obj_create(completion_popup);
    lv_obj_set_size(version_list_container, (int)(280 * scale),
                    (int)(200 * scale));
    lv_obj_align(version_list_container, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_flex_flow(version_list_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(version_list_container, (int)(5 * scale), 0);
    lv_obj_set_style_pad_row(version_list_container, (int)(5 * scale), 0);
    lv_obj_set_style_border_width(version_list_container, 0, 0);
    lv_obj_set_scrollbar_mode(version_list_container, LV_SCROLLBAR_MODE_ON);
    lv_obj_set_scroll_dir(version_list_container, LV_DIR_VER);
    lv_obj_add_flag(version_list_container, LV_OBJ_FLAG_SCROLLABLE);

    //Enter the version name of the updated check into the list
    int item_height = (int)(50 * scale);
    for (int i = 0; i < MAX_VERSION_COUNT; i++)
    {
        struct version_info temp_version;//Read out the flash information
        if (dfu_pan_get_version_info(i, &temp_version) != 0) {
            continue; 
        }
        
        const char *version_name;
        char version_text[32];

        //Set the displayed version name
        if (temp_version.name[0] != '\0')
        {
            version_name = temp_version.name;
        }
        else
        {
            version_name = "";
        }

        lv_obj_t *version_item =
            lv_list_add_btn(version_list_container, NULL, version_name);
        lv_obj_set_width(version_item, lv_pct(100));
        lv_obj_set_height(version_item, item_height);
        lv_obj_set_style_text_align(version_item, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_pad_all(version_item, (int)(8 * scale), 0);
        lv_obj_set_style_border_width(version_item, (int)(1 * scale), 0);
        lv_obj_set_style_border_color(version_item, lv_color_hex(0x00a0ff), 0);
        lv_obj_set_style_radius(version_item, (int)(4 * scale), 0);

        lv_obj_set_user_data(version_item, (void*)(uintptr_t)i);

        lv_obj_add_event_cb(version_item, version_button_event_cb,
                            LV_EVENT_CLICKED, NULL);
        if (temp_version.name[0] == '\0')
        {
            lv_obj_set_style_text_color(version_item, lv_color_hex(0x888888),
                                        0);
            lv_obj_set_style_bg_color(version_item, lv_color_hex(0x404040), 0);
        }
    }

    confirm_popup = lv_obj_create(lv_screen_active());
    lv_obj_set_size(confirm_popup, (int)(300 * scale), (int)(200 * scale));
    lv_obj_center(confirm_popup);
    lv_obj_set_style_bg_color(confirm_popup, lv_color_hex(0x202020), 0);
    lv_obj_set_style_border_color(confirm_popup, lv_color_hex(0x00a0ff), 0);
    lv_obj_set_style_border_width(confirm_popup, (int)(2 * scale), 0);
    lv_obj_set_style_radius(confirm_popup, (int)(10 * scale), 0);
    lv_obj_add_flag(confirm_popup, LV_OBJ_FLAG_HIDDEN);

    confirm_label = lv_label_create(confirm_popup);
    lv_obj_align(confirm_label, LV_ALIGN_TOP_MID, 0, (int)(30 * scale));
    lv_obj_set_style_text_color(confirm_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(confirm_label, &ota_font, 0);

    confirm_btn = lv_button_create(confirm_popup);
    lv_obj_set_size(confirm_btn, (int)(100 * scale), (int)(40 * scale));
    lv_obj_align(confirm_btn, LV_ALIGN_BOTTOM_MID, (int)(-60 * scale),
                 (int)(-20 * scale));
    lv_obj_add_event_cb(confirm_btn, confirm_btn_event_cb, LV_EVENT_CLICKED,
                        NULL);

    confirm_btn_label = lv_label_create(confirm_btn);
    lv_label_set_text(confirm_btn_label, "确认");
    lv_obj_center(confirm_btn_label);
    lv_obj_set_style_text_font(confirm_btn_label, &ota_font, 0);

    cancel_btn = lv_button_create(confirm_popup);
    lv_obj_set_size(cancel_btn, (int)(100 * scale), (int)(40 * scale));
    lv_obj_align(cancel_btn, LV_ALIGN_BOTTOM_MID, (int)(60 * scale),
                 (int)(-20 * scale));
    lv_obj_add_event_cb(cancel_btn, cancel_btn_event_cb, LV_EVENT_CLICKED,
                        NULL);

    cancel_btn_label = lv_label_create(cancel_btn);
    lv_label_set_text(cancel_btn_label, "取消");
    lv_obj_center(cancel_btn_label);
    lv_obj_set_style_text_font(cancel_btn_label, &ota_font, 0);
    rt_kprintf("Startup otaUI started\n");

    return RT_EOK;
}

void dfu_pan_ui_task(void *args)
{
    rt_kprintf("dfu_pan_ui_task start\n");
    rt_err_t ret = RT_EOK;
    rt_uint32_t ms;

    /* init littlevGL */
    ret = littlevgl2rtt_init("lcd");
    if (ret != RT_EOK)
    {
        rt_kprintf("littlevGL init failed\n");
        return;
    }

    ota_ui_msg_queue =
        rt_mq_create("ota_ui_msg", sizeof(ui_msg_t *), 20, RT_IPC_FLAG_FIFO);
    if (ota_ui_msg_queue == RT_NULL)
    {
        rt_kprintf("Failed to create UI message queue\n");
        return;
    }

    float scale = get_scale_factor();
    g_scale = scale;

    ret = ota_ui_obj_init(scale);
    if (ret != RT_EOK)
    {
        rt_kprintf("UI init failed\n");
        return;
    }

    while (1)
    {
        ui_msg_t *msg;
        while (rt_mq_recv(ota_ui_msg_queue, &msg, sizeof(ui_msg_t *), 0) ==
               RT_EOK)
        {
            if (msg == RT_NULL)
            {
                rt_kprintf("Received NULL message pointer\n");
                continue;
            }
            switch (msg->type)
            {
            case UI_MSG_UPDATE_BLE:
                if (msg->data)
                {
                    if (strcmp(msg->data, UI_MSG_DATA_BLE_CONNECTED) == 0)
                    {
                        lv_image_set_src(ui_ble, &ota_ble);
                    }
                    else if (strcmp(msg->data, UI_MSG_DATA_BLE_DISCONNECTED) ==
                             0)
                    {
                        lv_image_set_src(ui_ble, &ota_ble_close);
                    }
                }
                break;
            case UI_MSG_UPDATE_NET:
                if (msg->data)
                {
                    if (strcmp(msg->data, UI_MSG_DATA_NET_CONNECTED) == 0)
                    {
                        lv_image_set_src(ui_network, &ota_network_icon_img);
                    }
                    else if (strcmp(msg->data, UI_MSG_DATA_NET_DISCONNECTED) ==
                             0)
                    {
                        lv_image_set_src(ui_network,
                                         &ota_network_icon_img_close);
                    }
                }
                break;
            case UI_MSG_UPDATE_PROGRESS:
                if (msg->data)
                {
                    rt_kprintf("PROGRESS: update progress: %s\n", msg->data);
                    lv_arc_set_value(ui_progress, atoi(msg->data));
                    lv_label_set_text(ui_prolabel, msg->data);

                    lv_obj_set_style_arc_color(ui_progress,
                                               lv_color_hex(0x90EE90),
                                               LV_PART_INDICATOR);
                }
                break;
            case UI_MSG_UPDATE_PROGRESS_COLOR:
                if (msg->data)
                {
                    if (strcmp(msg->data, PROGRESS_COLOR_NORMAL) == 0)
                    {

                        lv_obj_set_style_arc_color(ui_progress,
                                                   lv_color_hex(0x00a0ff),
                                                   LV_PART_INDICATOR);
                    }
                    else if (strcmp(msg->data, PROGRESS_COLOR_SUCCESS) == 0)
                    {

                        lv_obj_set_style_arc_color(ui_progress,
                                                   lv_color_hex(0x90EE90),
                                                   LV_PART_INDICATOR);
                    }
                    else if (strcmp(msg->data, PROGRESS_COLOR_ERROR) == 0)
                    {

                        lv_obj_set_style_arc_color(ui_progress,
                                                   lv_color_hex(0xFF0000),
                                                   LV_PART_INDICATOR);
                    }
                }
                break;
            case UI_MSG_UPDATE_FILES:
                if (msg->data)
                {
                    lv_label_set_text(ui_status, msg->data);
                }
                break;
            case UI_MSG_UPDATE_BUTTON:
                if (msg->data)
                {

                    lv_label_set_text(ui_update, msg->data);
                }
                break;
            case UI_MSG_UPDATE_BUTTON_COLOR:
                if (msg->data)
                {
                    if (strcmp(msg->data, PROGRESS_COLOR_SUCCESS) == 0)
                    {

                        lv_obj_set_style_arc_color(ui_update,
                                                   lv_color_hex(0x90EE90),
                                                   LV_PART_INDICATOR);
                    }
                    else if (strcmp(msg->data, PROGRESS_COLOR_ERROR) == 0)
                    {

                        lv_obj_set_style_arc_color(ui_update,
                                                   lv_color_hex(0xFF0000),
                                                   LV_PART_INDICATOR);
                    }
                }
                break;
            case UI_MSG_SHOW_FAILURE_POPUP:
                if (confirm_popup)
                {

                    lv_label_set_text(confirm_label, "OTA更新失败！");
                    lv_label_set_text(confirm_btn_label, "重试");
                    lv_label_set_text(cancel_btn_label, "取消");
                    lv_obj_remove_flag(confirm_popup, LV_OBJ_FLAG_HIDDEN);
                }
                break;
            case UI_MSG_SHOW_SUCCESS_POPUP:
                if (confirm_popup)
                {
                    lv_label_set_text(confirm_label, "OTA更新成功");
                    lv_label_set_text(confirm_btn_label, "重启");
                    lv_label_set_text(cancel_btn_label, "取消");
                    lv_obj_remove_flag(confirm_popup, LV_OBJ_FLAG_HIDDEN);
                }
                break;
            case UI_MSG_SHOW_NETWORK_ERROR_POPUP:

                break;
            case UI_MSG_SHOW_VERSION_CONFIRM:
                if (selected_version_index >= 0 &&
                    selected_version_index < MAX_VERSION_COUNT)
                {
                    struct version_info temp_version;
                    if (dfu_pan_get_version_info(selected_version_index, &temp_version) == 0 &&
                        temp_version.name[0] != '\0') {
                        rt_kprintf("Selected version: %s\n", temp_version.name);
                        lv_obj_add_flag(completion_popup, LV_OBJ_FLAG_HIDDEN);

                        snprintf(g_confirm_text_buffer,
                                 sizeof(g_confirm_text_buffer), "确认更新至%s？",
                                 temp_version.name);
                        lv_label_set_text(confirm_label, g_confirm_text_buffer);

                        lv_obj_remove_flag(confirm_popup, LV_OBJ_FLAG_HIDDEN);
                    }
                }
                break;
            case UI_MSG_START_OTA_UPDATE:

                lv_obj_add_flag(confirm_popup, LV_OBJ_FLAG_HIDDEN);

                lv_obj_add_flag(completion_popup, LV_OBJ_FLAG_HIDDEN);

                if (selected_version_index >= 0 &&
                    selected_version_index < MAX_VERSION_COUNT)
                {
                    struct version_info temp_version;
                    if (dfu_pan_get_version_info(selected_version_index, &temp_version) == 0) {
                        rt_kprintf("Starting OTA update for version: %s\n",
                                   temp_version.name);
                        rt_kprintf("URL: %s\n", temp_version.zippath);
                        lv_label_set_text(Update_sign, "正在更新");
                        lv_obj_set_style_bg_color(ui_update, lv_color_hex(0x90EE90),
                                                  LV_PART_MAIN);
                        rt_err_t result =
                            dfu_pan_create_worker_thread(selected_version_index);
                        if (result != RT_EOK)
                        {
                            rt_kprintf("Failed to create OTA worker thread\n");
                        }
                        else
                        {
                            rt_kprintf("OTA worker thread created successfully\n");
                        }
                    }
                }
                break;
            }

            if (msg->data)
            {
                ui_free(msg->data);
            }
            rt_free(msg);
        }

        ms = lv_task_handler();
        rt_thread_mdelay(ms);
    }
}