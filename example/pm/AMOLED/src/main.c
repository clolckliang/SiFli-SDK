#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include <string.h>
#include "drv_lcd_private.h"
#include "mem_section.h"
#include "bf0_pm.h"
#include "drv_gpio.h"
#include "gui_app_pm.h"

#define TEST_FRAMEBUFFER
#define DISPLAY_DURATION_MS 5000  // Display duration: 5 seconds
#define DISPLAY_INTERVAL_MS 1000   // Refresh interval: 1 second
// Low power mode definitions
typedef enum
{
    POWER_MODE_DISPLAY_OFF = 0,  // Display off low power mode
    POWER_MODE_DISPLAY_ON        // Display on low power mode
} power_mode_t;

static volatile int wakeup_flag = 0;
static power_mode_t current_power_mode = POWER_MODE_DISPLAY_OFF;  // Default to display off mode

// Event object
static rt_event_t display_event;

// Default key pin definition (please modify according to actual hardware)
#ifndef BSP_KEY_WAKEUP_PIN
    #define BSP_KEY_WAKEUP_PIN BSP_KEY1_PIN
#endif

static struct rt_device_graphic_info lcd_info;
static rt_tick_t display_start_time = 0;  // Display start time
static rt_device_t lcd_device_global = RT_NULL;  // Global LCD device pointer
static int system_sleeping = 0;  // System sleep state flag

#ifdef TEST_FRAMEBUFFER
    L2_NON_RET_BSS_SECT_BEGIN(frambuf)
    // Use a smaller framebuffer, e.g., 1/4 of screen size
    #define FRAMEBUFFER_WIDTH  (LCD_HOR_RES_MAX/2)
    #define FRAMEBUFFER_HEIGHT (LCD_VER_RES_MAX/2)
    L2_NON_RET_BSS_SECT(frambuf, ALIGN(64) static char frambuffer[FRAMEBUFFER_WIDTH * FRAMEBUFFER_HEIGHT * 3]);
    L2_NON_RET_BSS_SECT_END
#else
    #ifdef  PSRAM_BASE
        // Ensure PSRAM buffer has proper alignment
        const char *frambuffer = (const char *)BSP_USING_PSRAM;
    #else
        const char *frambuffer = (const char *)HPSYS_RAM0_BASE;
    #endif /* PSRAM_BASE */
#endif

// Convert RGB888 to specific format color
static uint32_t make_color(uint16_t cf, uint32_t rgb888)
{
    uint8_t r, g, b;

    r = (rgb888 >> 16) & 0xFF;
    g = (rgb888 >> 8) & 0xFF;
    b = (rgb888 >> 0) & 0xFF;

    switch (cf)
    {
    case RTGRAPHIC_PIXEL_FORMAT_RGB565:
        return ((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | (((b) & 0xF8) >> 3));
    case RTGRAPHIC_PIXEL_FORMAT_RGB888:
        return ((((r) & 0xFF) << 16) | (((g) & 0xFF) << 8) | ((b) & 0xFF));
    default:
        return 0;
    }
}

// Fill color to buffer
static void fill_color(uint8_t *buf, uint32_t width, uint32_t height,
                       uint16_t cf, uint32_t ARGB8888)
{
    uint8_t pixel_size;

    if (RTGRAPHIC_PIXEL_FORMAT_RGB565 == cf)
    {
        pixel_size = 2;
    }
    else if (RTGRAPHIC_PIXEL_FORMAT_RGB888 == cf)
    {
        pixel_size = 3;
    }
    else
    {
        RT_ASSERT(0);
    }

    uint32_t i, j, k, c;
    c = make_color(cf, ARGB8888);
    for (i = 0; i < height; i++)
    {
        for (j = 0; j < width; j++)
        {
            for (k = 0; k < pixel_size; k++)
            {
                *buf++ = (c >> (k << 3)) & 0xFF;
            }
        }
    }
}

// Generate random color
static uint32_t rand_color(void)
{
    return (rt_tick_get() * 0x12345) & 0xFFFFFF; // Use system tick to generate pseudo-random color
}

// Drawing function
static void draw(rt_device_t lcd_device)
{
    // Fill entire screen with random color
#ifdef TEST_FRAMEBUFFER
    fill_color((uint8_t *)frambuffer, lcd_info.width, lcd_info.height,
               (16 == lcd_info.bits_per_pixel) ? RTGRAPHIC_PIXEL_FORMAT_RGB565 : RTGRAPHIC_PIXEL_FORMAT_RGB888,
               rand_color());
#endif /* TEST_FRAMEBUFFER */

    // Set drawing window to full screen
    rt_graphix_ops(lcd_device)->set_window(0, 0, lcd_info.width - 1, lcd_info.height - 1);

    // Draw entire screen synchronously
    rt_graphix_ops(lcd_device)->draw_rect((const char *)frambuffer, 0, 0, lcd_info.width - 1, lcd_info.height - 1);

    rt_kprintf("Screen updated at tick: %d\n", rt_tick_get());
}

// Key interrupt callback function
static void key_wakeup_callback(void *args)
{

    if (system_sleeping)
    {
        rt_kprintf("Key pressed, waking up system...\n");
        rt_pm_request(PM_SLEEP_MODE_IDLE); // Request idle mode
        // Send event notification instead of setting flag
        rt_event_send(display_event, 0x01);
        system_sleeping = 0;
    }
}

// Initialize key wakeup function
static void init_key_wakeup(void)
{
    int8_t wakeup_pin;
    uint16_t gpio_pin;
    GPIO_TypeDef *gpio;

    // Get GPIO instance and pin
    gpio = GET_GPIO_INSTANCE(BSP_KEY_WAKEUP_PIN);
    gpio_pin = GET_GPIOx_PIN(BSP_KEY_WAKEUP_PIN);

    // Query wakeup pin
    wakeup_pin = HAL_HPAON_QueryWakeupPin(gpio, gpio_pin);

    RT_ASSERT(wakeup_pin >= 0);
    // Enable key wakeup function
    pm_enable_pin_wakeup(wakeup_pin, AON_PIN_MODE_DOUBLE_EDGE); // Double edge trigger

    // Configure key GPIO
    rt_pin_mode(BSP_KEY_WAKEUP_PIN, PIN_MODE_INPUT);                                       // Configure as input mode
    rt_pin_attach_irq(BSP_KEY_WAKEUP_PIN, PIN_IRQ_MODE_RISING_FALLING, key_wakeup_callback, RT_NULL); // Configure interrupt callback function, rising and falling edge trigger
    rt_pin_irq_enable(BSP_KEY_WAKEUP_PIN, PIN_IRQ_ENABLE);
}

// Enter sleep mode function - choose whether to turn off LCD based on mode
static void enter_sleep_mode(rt_device_t lcd_device)
{
    if (current_power_mode == POWER_MODE_DISPLAY_OFF)
    {
        // Display off sleep mode: Close LCD device
        rt_device_control(lcd_device, RTGRAPHIC_CTRL_POWEROFF, NULL);
#ifdef BSP_USING_TOUCHD
        lcd_device = rt_device_find("touch");
        if (lcd_device)
        {
            rt_device_control(lcd_device, RTGRAPHIC_CTRL_POWEROFF, NULL);
        }
#endif
        rt_kprintf("LCD closed, entering sleep mode (display off)...\n");
    }
    else
    {
        // Display on sleep mode: Keep LCD on
        const uint8_t idle_mode_on = 1;
        rt_device_control(lcd_device, RTGRAPHIC_CTRL_SET_MODE, (void *)&idle_mode_on);
        rt_kprintf("Entering sleep mode (display on)...\n");
    }

    // Set flag before entering sleep via GUI method
    system_sleeping = 1;  // Ensure flag is set before entering sleep
}

// Reinitialize LCD after wakeup (only needed in display off mode)
static rt_err_t reinit_lcd(rt_device_t lcd_device)
{
    if (current_power_mode == POWER_MODE_DISPLAY_OFF)
    {
        rt_device_control(lcd_device, RTGRAPHIC_CTRL_POWERON, NULL);

        // Re-set framebuffer format
        uint16_t cf;
        if (16 == lcd_info.bits_per_pixel)
            cf = RTGRAPHIC_PIXEL_FORMAT_RGB565;
        else if (24 == lcd_info.bits_per_pixel)
            cf = RTGRAPHIC_PIXEL_FORMAT_RGB888;
        else
        {
            rt_device_close(lcd_device);
            return -RT_ERROR;
        }
        rt_device_control(lcd_device, RTGRAPHIC_CTRL_SET_BUF_FORMAT, &cf);

        return RT_EOK;
    }
    return RT_EOK;
}

// Main task function
static void lcd_display_task(void *parameter)
{

    rt_device_t lcd_device = (rt_device_t)parameter;
    rt_err_t result;

    // Save global LCD device pointer
    lcd_device_global = lcd_device;

    // Create event object
    display_event = rt_event_create("display", RT_IPC_FLAG_FIFO);
    RT_ASSERT(display_event);

    // Open LCD device
    result = rt_device_open(lcd_device, RT_DEVICE_OFLAG_RDWR);
    RT_ASSERT(result == RT_EOK);

    // Get LCD information
    if (rt_device_control(lcd_device, RTGRAPHIC_CTRL_GET_INFO, &lcd_info) != RT_EOK)
    {
        rt_kprintf("Failed to get LCD info\n");
        rt_device_close(lcd_device);
        return;
    }
    // Set framebuffer format
    uint16_t cf;
    if (16 == lcd_info.bits_per_pixel)
        cf = RTGRAPHIC_PIXEL_FORMAT_RGB565;
    else if (24 == lcd_info.bits_per_pixel)
        cf = RTGRAPHIC_PIXEL_FORMAT_RGB888;
    else
    {
        rt_kprintf("Unsupported pixel format\n");
        rt_device_close(lcd_device);
        return;
    }
    rt_device_control(lcd_device, RTGRAPHIC_CTRL_SET_BUF_FORMAT, &cf);

    while (1)
    {
        rt_pm_request(PM_SLEEP_MODE_IDLE); // Request idle mode

        // Record display start time
        display_start_time = rt_tick_get();

        // Main loop: Draw every second for specified duration
        while (1)
        {
            rt_tick_t current_time = rt_tick_get();
            rt_tick_t elapsed_time = current_time - display_start_time;

            // Check if specified time has elapsed
            if (elapsed_time >= rt_tick_from_millisecond(DISPLAY_DURATION_MS))
            {
                rt_kprintf("Display period finished, entering sleep mode...\n");
                break;  // Exit loop, prepare to enter sleep
            }
            draw(lcd_device);

            // Delay 1 second
            rt_thread_mdelay(1000);
        }
        // Enter sleep mode
        enter_sleep_mode(lcd_device);
        rt_pm_release(PM_SLEEP_MODE_IDLE); // Release idle mode request
        // Wait for wakeup event
        rt_uint32_t event_flag;
        rt_err_t result = rt_event_recv(display_event, 0x01,
                                        RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR,
                                        RT_WAITING_FOREVER, &event_flag);

        if (result == RT_EOK)
        {
            result = reinit_lcd(lcd_device);
            if (result != RT_EOK)
            {
                rt_thread_mdelay(10000);
                continue;
            }
        }
    }
}

// Finsh command: Set low power mode
static int power_mode_set(int argc, char **argv)
{
    int mode = atoi(argv[1]);
    if (mode == 0)
    {
        current_power_mode = POWER_MODE_DISPLAY_OFF;
        rt_kprintf("Power mode set to: DISPLAY OFF (LCD will be closed during sleep)\n");
    }
    else if (mode == 1)
    {
        current_power_mode = POWER_MODE_DISPLAY_ON;
        rt_kprintf("Power mode set to: DISPLAY ON (LCD will remain on during sleep)\n");
    }
    else
    {
        return -1;
    }

    return 0;
}
MSH_CMD_EXPORT_ALIAS(power_mode_set, power_mode, Set power mode: power_mode < 0 | 1 >);

// Initialization function
int lcd_simple_display_init(void)
{
    rt_device_t lcd_device = rt_device_find("lcd");
    RT_ASSERT(lcd_device);
    lcd_display_task(lcd_device);
    return 0;
}
int main(void)
{
    HAL_LPAON_Sleep();

    // Initialize key wakeup function
    init_key_wakeup();
    // Initialize GUI
    gui_ctx_init();

    // Initialize display task
    if (lcd_simple_display_init() != 0)
    {
        rt_kprintf("Failed to initialize display task\n");
    }
    // Main thread can perform other tasks or enter idle state
    while (1)
    {
        rt_thread_mdelay(10000);
    }

    return RT_EOK;
}