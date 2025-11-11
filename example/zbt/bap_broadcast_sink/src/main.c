#include "bap_broadcast_sink_api.h"
#include "log.h"

#define DEFAULT_VOLUME 10

int main(void)
{
    int err;

    err = bt_enable(NULL);
    if (err)
    {
        printk("Bluetooth enable failed (err %d)\n", err);
        return err;
    }
    printk("Bluetooth initialized\n");

    //rt_thread_mdelay(1000);

    err = bap_broadcast_sink_init();
    if (err)
    {
        printk("Init failed (err %d)\n", err);
        return -1;
    }

    audio_server_set_private_volume(AUDIO_TYPE_BT_MUSIC, DEFAULT_VOLUME);
    while (1)
    {
        err = bap_broadcast_sink_start();
        if (err)
        {
            printk("start bap sink failed\n");
            rt_thread_mdelay(100);
            continue;
        }
        break;
    }

    return 0;
}

/*cmd help:
 * audio_src 0    //stop
 * audio_src 1    //start with stream 0
 * audio_src 1 0  //start with stream 0
 * audio_src 1 1  //start with stream 1
 */
void audio_src(int argc, char **argv)
{
    if (argc < 2)
        return;

    if (argv[1][0] == '0')
    {
        bap_broadcast_sink_stop();
        printk("\r\ninput audio_src 1 0 to start steram 0\n\n");
        printk("\r\ninput audio_src 1 1 to start stream 1\n\n");
    }
    else
    {
        int idx = 0;
        if (argc >= 3 && argv[2][0] == '1')
            idx = 1;
        if (CONFIG_BT_BAP_BROADCAST_SNK_STREAM_COUNT == 1)
        {
            idx = 0;
        }
        bap_broadcast_sink_start(idx);

        printk("\r\ninput audio_src 0 to stop\n\n");
    }
}
MSH_CMD_EXPORT(audio_src, audio_src command)

#if defined(SF32LB52X_58)|| (defined(SF32LB52X) && (defined(SF32LB52X_REV_B) || defined(SF32LB52X_REV_AUTO)))
uint16_t g_em_offset[HAL_LCPU_CONFIG_EM_BUF_MAX_NUM] =
{
    0x178, 0x178, 0x740, 0x7A0, 0x848, 0x8B8, 0xB38, 0xCE8, 0xE80, 0x1474,
    0x14DC, 0x1AF4, 0x22F4, 0x2374, 0x23EC, 0x301C, 0x301C, 0x301C, 0x301C, 0x301C,
    0x32BC, 0x32DC, 0x333C, 0x338C, 0x34AC, 0x34C0, 0x34D4, 0x35C4, 0x35D4, 0x3654,
    0x3670, 0x4680, 0x5690, 0x5F00,
};

void lcpu_rom_config(void)
{
    uint8_t is_config_allowed = 0;
#ifdef SF32LB52X
    uint8_t rev_id = __HAL_SYSCFG_GET_REVID();
    if (rev_id >= HAL_CHIP_REV_ID_A4)
        is_config_allowed = 1;
#elif defined(SF32LB52X_58)
    is_config_allowed = 1;
#endif

    extern void lcpu_rom_config_default(void);
    lcpu_rom_config_default();

    if (is_config_allowed)
    {
        hal_lcpu_bluetooth_em_config_t em_offset;
        memcpy((void *)em_offset.em_buf, (void *)g_em_offset, HAL_LCPU_CONFIG_EM_BUF_MAX_NUM * 2);
        em_offset.is_valid = 1;
        HAL_LCPU_CONFIG_set(HAL_LCPU_CONFIG_BT_EM_BUF, &em_offset, sizeof(hal_lcpu_bluetooth_em_config_t));

        hal_lcpu_bluetooth_act_configt_t act_cfg;
        act_cfg.bt_max_acl = 2;
        act_cfg.bit_valid = (1 << 0);
        HAL_LCPU_CONFIG_set(HAL_LCPU_CONFIG_BT_ACT_CFG, &act_cfg, sizeof(hal_lcpu_bluetooth_act_configt_t));
    }
}
#endif


