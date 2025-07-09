# SiFli SDK change Log v2.4.1
## Change log since v2.4.0
### Bluetooth
#### Fixed
- ble: Fixed the error of bass profile to handle level notify.
- bt: Fixed the parameter error in sco cfm event of HFP AG profile.
- bt: Fixed read BT rssi always return invalid value.
- ble: Fixed that gatt service registerred multi times if tran_service register more than one time.
- ble: Fixed a bug that tran_client could not handle data length more than MTU.
- bt: Fixed BT ACL is stuck if highest 8 bits of addr.lap is not NULL.
- bt: Fixed a HFP AG profile connection collision bug.
- bt: Fixed a bug that HID may not reconnect if disconnected due to abnormal disconnection.
- ble: Fixed a bug that ke_malloc encountered assert due to using extra memory pool.

#### Changed
- bt: Optimize bt_connection_manager to spilit with turn key solution.
- common: Optimize 48M calibration for product verification.
- common: Enable secure connection in host.

#### Added
- bt: HFP AG role support multi connection.
- bt: Added new API bt_interface_rd_extend_feature() and event BT_NOTIFY_COMMON_RD_EXT_FEATURE_RSP to get remote extended feature.
- ble: Add new event SIBLES_ATT_ERROR_EVENT_IND to indicate gatt error.

### Drivers
#### Fixed
- mpi: Fix DMA IRQ is not disabled after flash write
- tim: Fix BTIM clock is not enabled
- pmu: Fix hcpu wakeup crash on sf32lb56
- epic: Supports drawing to ARGB buffer in render-list mode
- epic: Fix EPIC coordinates overflow in 'HAL_EPIC_Copy_IT'
- epic: YUV image is all black in EPIC render-list mode.

#### Changed
- touch: Not enable touch irq data service by default
- lcd: Allow user to use the LCD modules defined within their own projects (i.e. outside sdk folder)
- pmu: Turnoff LDO2 to ensure flash changing to 3byte address mode when boot up

### Middleware
#### Fixed
- vglite: Fix vglite not work as semaphore create fails
- audio: Fix 58x A1 chip no sound call issue

#### Changed
- data_service: Not enable mbox thread by default for sf32lb52
- dfu: Change dfu image location in ftab to avoid conflict with ACPU image
- audio: Increase PA delay to 200ms

### Examples
#### Fixed
- `rt_device/pwm`: Fix doc title
- `rt_device/spi_tf`: Fix crash on board sf32lb52-lchspi-ulp
- `ble/hid`: Fix PAN reconnection fail
- `rt_device/audprc`: Fix loopback command error in README
- `boot_loader`: Fix ACPU code is not loaded

#### Changed
- `ble/hid`: Add PC support and support physical button
- `get-started/hello_world`: Add simulator support
- `get-started/hello_world`: Make finsh work
- `multimedia/lvgl/watch`: Increase hcpu_ram_data_size for sf32lb52 related board
- `storage/fatfs`: Update README

#### Added
- Add pingpong OTA example in `ble/central_and_peripheral_with_pingpong_ota`
- Add SPP example in `bt/spp`

### BSP
#### Fixed
- board: Fix lcpu console port config error of board 58-lcd_a128r32n1, 58-lcd_n16r32n1 and 58-lcd_n16r64n4
- MMC56x3: Fix MMC56x3 failing to turn on continuous mode properly

#### Changed
- board: Optimize power consumption by updating pinmux setting of board sf32lb52-lchspi-ulp
- board: Enable Windows simulator by default in board pc

#### Added
- board: Add ec-lb587_a2 for 587evb board using A2 chip

### Tools
#### Fixed
- UartburnEx: Fix nor flash id error by updating UartburnEx to v1.5
- ImgDownUart: Fix eMMC download fail
- build: Fix keil project generation fail
- build: Fixed an issue where you sometimes needed to run export twice.
- build: Fix generated cppdefines.txt

#### Changed
- build: Change Keil used C++ standard to ISO C++ 2020
- png2ezip: update ezip version to 2.3.4 to support lvgl v9

### Docs
#### Fixed:
- example: Fix some example doc missing

#### Added
- Add coding-style
- Add contribution guide
- Add feedback submission functionality

## Change log since v2.3

## Major Changes
- Update toolchain to support Linux and macOS. Legacy env tool (upgraded to v1.1.2) could still be used on Windows. But it's recommended to use native terminal tool (i.e. PowerShell for Windows) instead of env for development on Windows. Please refer to [install guide](https://docs.sifli.com/projects/sdk/v2.4/sf32lb52x/quickstart/install/script/index.html). New terminal toolchain doesn't support SDK version `<v2.4`. Windows env tool could support all SDK versions.
- Upgrade GCC to GCC 14. Windows env tool contains both GCC 8 and GCC 14. GCC 8 is used by SDK version `<v2.4`. GCC 14 is used by SDK version `>=v2.4`.
- Upgrade Python to Python 3.11. Windows env tool contains both Python 2.7 and Python 3.11. Python 2.7 is used by SDK version `<v2.4`. Python 3.11 is used by SDK version `>=v2.4`
- `middleware/bluetooth` is integrated as submodule. Downloaded zip file doesn't contain the complete code anymore
- Support SF32LB58 A2 chip. SF32LB58 EVB board is configured with A1 chip. SF32LB58-LCD board is configured with A2 chip.

### Bluetooth
#### Fixed
- bt: Fix sf32lb58 BT sniff disconnect issue
- common: Reset 56x bt lcpu audio buffer for cvsd current sound
- common: Fixed some controller bugs
- ble: Fixed BLE write packet wrong order issue
- common: Increase stack ready wait time from 5s to 8s
- bt: Fixed connection fail with some phones on sf32lb52
- bt: Fixed l2cap connection failed with some Qualcomm peripherial devices
- common: Fixed a memory leak of rfcomm
- ble: Fixed gatt search failed if remote device has duplicate gatt characteristic
- ble: Fixed gatt search descriptor failed if remote device has more than 2 descriptor
- ble: Fixed Periodic advertising carried wrongly TX power.
- bt: Fixed volume can not set for some earphones
- ble: Fixed update connection parameter failed in sf32lb55x
- ble: Fixed advertise length check error in the customize mode of advertising service
- bt: Fixed the time of connecting to oppo R15 is too long
- ble: Fixed sync state is not clear after active stopped periodic adv scan



#### Changed
- bt: Optimize 3sco
- common: Disable sf32lb58 BTC warning assert
- rf: Update RF driver for sf32lb58 and sf32lb52
- test: Fix userbin nonsignaling test power issue
- rf: Fix 6dBm power issue
- bt: Optimize events of L2CAP customized profile and HF AG profile
- ble: Optimize comments for event of periodic sync stop
- bt: Optimize bt pan flow to avoid some potential problems
- ble: Do not resume periodic sync in the resume process of BLE pairing.
- bt: Do not switch role if connected more than 2 peripherial devices.
- bt: Optimize the structure of address in SPP APIs
- bt: Remove reconnection timer in bt connection manager
- common: Optimize configure of sf32lb58 lcpu




#### Added
- common: Add 587 PTA for coexistence with WIFI
- rf: Add BT RF CW test interface for sf32lb56x and sf32lb58x
- common: Add new event to notify extended features of remote device
- bt: Add SPP loop-back function
- bt: Add new API to set link policy
- bt: Add BT HID example
- ble: Add new API ble_gap_scan_start_ex() to set active/passive property
- bt: Add new APIs in bt connection mananger to passkey entry pair
- bt: Support AAC decode in A2DP
- bt: Add new APIs to config BT io capability and response to user confirm
- bt: SPP support multi channels and links






### Drivers
#### Fixed
- usb: Fix DP still in PULLUP state after USB is disabled
- lcd: Fixed drv_lcd_fb lock error
- aon: Fix pin mode is not configured correctly for wakeup pin greater than PIN15 on sf32lb52x
- adc: Disable ADC timer trigger by default
- adc: Fixed crash when clock change on sf32lb52x if no adc instance is enabled
- pmu: Fix the problem of inconsistent parameter passing types for the `round` function
- spi: Add 3wire support of SPI master
- pwm: Optimize PWM  pulse percentage 0% and 100% waveform
- pinmux: Fix PBR PULLDOWN set failure on sf32lb56x
- spi: Fix SPI set clk div fail sometimes on sf32lb56x
- gpio: Fix memory overwritten as i is not initialized
- sdhci: Add eMMC low power mode support
- mailbox: Fix HAL_MAILBOX_LockEx timeout doesn't work on LCPU
- usb: Fix misalignment
- lcd: Fix the first frame doesn't appear on LCD sometimes
- lcd: Enable LCDC default layer
- epic: Fixed 'cont_blend_reset' error
- epic: Fixed 'drv_epic_fill' fore layer is not initialized
- audcodec: Fix PLL calibration of sf32lb58
- nand: Fixed using wrong NAND address by mtd device if RT_DFS_ELM_DHARA_ENABLED is not enabled
- aon: Fixed HAL_LPAON_DisableWakeupSrc not work on LCPU of sf32lb55x
- nor: Fix XT25F128F's type and delete duplicate BY25Q128ES in type 2
- wdt: Fix wdt start/stop fail.
- spi_tf: Fix some card cannot be detected in SPI mode
- ld: Fix DPI screen flicker of sf32lb56-lcd board
- nor: Fix sf32lb52 SIP flash read id error
- epic: Fix the drv_epic D-cache cleaning issues.
- epic: Fixed rotation3d abnormal



#### Changed
- dma: Add all DMA request definition
- lcd: Optimize drv_lcd_fb management 2 buffer
- lcd: Add an console cmd to skip LCD draw_core
- epic: Support customized viewpoint in epic-adv-type2
- lcd: Fixed DCX output error while LCDC reading SPI data
- epic: Update drv_epic statistics
- lcd: Move Dcache-clean out of HAL_LCDC
- pinmux: Unify pad numbering of hpsys and lpsys pad and no need to specify core in API `HAL_PIN_Set`
- audprc Add dual MIC support 
- lcd: Change DPI RGB565 output pins
- lcd: Use EXTDMA in preference to AES when they are both available
- adc: Add VBAT channel in multi-channel sampling mode for sf32lb52


#### Added
- mtd: Add mtd_dhara device to make fatfs support both NAND and NOR at run time
- epic: Add render_list support
- epic: Optimize EPIC filling translucent color

### Middleware
#### Fixed
- audio: start rx after tx interrupt immediately
- flashdb: Fixed fwrite in libc file mode
- flashdb: Add flash4 support
- agif: Fixed file mode bug
- lvgl: Fix mask_cf is not set
- sys: Fixed sifli_memset write data error
- audio: Fix wav parse error : wav_read_header return -1
- audio: Fix mp3 deocde error.
- ffmpeg: Fixed network error code
- lvgl(v8): Invalidate whole screen if using 2 PSRAM screen sized buffer
- lvgl: Fixed 24bit LVGL simulator screen messed up
- audio: Fix ffmpeg play mp3 file crash
- lvgl: Fixed 16bit LVGL simulator black screen
- lvgl: Fixed LV_LV_USE_L8_GPU not defined
- audio: Fix audio 3a open/close policy
- audio: Close audio 3a after client closed
- audio: Fix some audio server bugs
- lvgl: Fix LVGL 9 simulator wrong screen size
- media: Fix media player eixt memory leak

#### Changed
- audio: Fix simulator compile issue
- acpu_ctrl: Add new API acpu_send_result2 to report error_code
- coremark: Coremark package can be used without rt-thread enabled
- freetype: Open source lv_freetype.c to solve GCC build problem
- freetype: Add tiny_font lib for GCC and support big size font
- dfu: Update for NOR/NAND Flash support
- graphics: Gaussian blur supports A8 format
- audio: Support mp3 seek function
- audio: Make rx/tx device can be selected by user within i2s, pdm and audprc
- audio: Not use msbc for non-bt_voice
- lvgl: TWO_LCD_FRAMEBUFFER mode support partial update
- media: Optimize ezip memory usage
- lvgl: Use HW blending for A1/A2/A4 fonts
- rpmsg_lite: Make rpmsg-lite independent to RT_EVENT

#### Added
- secboot: Add secboot module for bootloader
- audio: Add DRC support
- audio: Add BLE talk
- audio: Add VBE(Virtual Bass Enhancement) support
- mbedtls: Add mbedtls v2.28.1
- audio: Add mix support
- lvgl(v8): Use render_list mechanism to improve performance
- lwip: Add lwip v2.1.2
- audio: Enable ogg and vorbis in ffmpeg
- audio: Add LE Audio device
- graphics: Add AGIF support
- graphics: Add JpegNanoD(JPEG Decoder) for sf32lb58
- graphics: Add JpegNanoE(JPEG Encoder) for sf32lb58
- graphics: Add vglite for sf32lb58
- audio: Support MICBIAS pin is used as power pin


### RTOS
#### Fixed
- Fix saved_stack_pointer if it's not aligned to 8byte

#### Changed
- timer: Increase timer task stack size from 512byte to 1024byte for HCPU
- pm: Use deep sleep mode by default if it is 52x


### Examples
#### Fixed
- `storage/flashdb`: Fixed print error
- `multimedia/lvgl/watch`: Support board `eh-lb551` and `eh-lb555`
- `multimedia/lvgl/watch`: Watch example support GCC and use tiny_full font
- `multimedia/lvgl/watch`: Support GCC toolchain and fixed cube rotation UI abnormal
- `multimedia/lvgl/watch`: Support GCC toolchain and fixed cube rotation UI abnormal
- `multimedia/lvgl/watch`: Fixed rotation3d abnormal
- `pm/coremark`: Fixed whileloop current abnormal on sf32lb52x when built by GCC
- `pm/coremark`: Fixed crash if building using GCC on sf32lb58x board
- boot: Fix 2nd bootloader crash on board 567-evb
- `bt/pan`: Fixed reconnect timer may trigger PAN connection even PAN already connected in `bt/pan` project.
- `pwm/pwm`: Fix pwm duty cycle 100%
- `hal_example`: Fix flash test fail on 583/587 evb
- `rt_device/pdm`: Fix pdm gain configuration error.

#### Changed
- bootloader: sf32lb58x 2nd boot not use rt-thread
- bootloader: Add DFU support
- `bt/music_source`: Support music file saved in NOR Flash filesystem
- `bt/central_and_peripheral`: Optimize project structure
- `multimedia/lvgl/watch`: Add simulator support
- `multimedia/lvgl/watch`: Optimize fps
- `multimedia/lvgl/watch`: Fixed mainmenu layout error on sf32lb55x
- `bt/pan`: Add OTA support


#### Added
- Added ble throughput example  in `ble/throughput`
- Added OTA example for sf32lb56x/sf32lb52x by `ble/peripheral_with_ota`
- Added BLE periodic adv example in `ble/periodic_adv`
- Added BT music sink example in `bt/music_sink`
- Added BT PAN example in `bt/pan`
- Add BT music source example in `bt/music_source`
- Add BT hands-free profile example in `bt/hfp`
- Added HCI over UART example in `bt/HCI_over_uart`
- Added pm example in `rt_device/pm`
- Added audprc example in `rt_device/audprc`
- Add I2S example in `rt_device/i2s`
- Add SPI TF card example in `rt_device/spi_tf`
- Add PDM example in `rt_device/pdm`
- Add RT_DEVICE GPIO example in `rt_device/gpio`
- Add PWM DMA example in `rt_device/pwm/pwm_dma`
- Add eMMC example in `rt_device/emmc`
- Added local music example in `multimedia/audio/local_music`
- Added record example in `multimedia/audio/record`
- Add JPEG decoder example in `jpeg_nanod`
- Add LCD stress test example in `lcd_stress_test`
- Add fatfs example in `storage/fatfs` for NOR and NAND Flash
- Added FlashDB example in `storage/flashdb`
- Add finsh example in `system/finsh`
- Add ulog example in `system/ulog`



### Tools
#### Fixed
- svd: Fix parsing error by some tool
- svd: Add sf32lb55/sf32lb58 svd and update for sf32lb52/sf32lb56
- build: Fixed build error if `.git` folder is present in application folder
- build: Fixed `GenDownloadScript` typo


#### Changed
- build: Support acpu image embedded in hcpu image
- build: Fix get SIFLI SDK build sha path
- Add crash_dump_analyzser install guide
- jlink_drv: Fix all zero data fail to be programmed into SD/eMMC
- jlink_drv: Add 58x emmc/sd1 jlink drv and project
- build: Print hint if board path is not present
- ImgDownUart: Update to support sf32lb58x
- FsrwTool: Update to support upload/download file by UART
- Add Trace32 download link
- bin2bmp: Supports changing the foreground color of Ax
- build: Add `buildlib` argument in `PrepareBuilding` and `board` argument in `PrepareEnv` to make lib build command simple
- build: Move environment construct to `PrepareBuilding` to unify SConstruct for simulator and other board
- jlink_drv: Add PMIC and customized control pin support
- font2c: Upgrade to support multiple freetype fonts register
- uart_download: Add verification step
- build: Add ftab support for sf32lb55 build
- uartServer: Upgrade SifliUsartServer to V2.8
- build: Add board search path to support custom boards



### BSP
#### Fixed
- board: Fix flash5 is not re-initialized by LCPU of board sf32lb58-lcd
- board: Fix wrong sf32lb58 board config causing deepwfi crash
- board: Fix flash1 of 585-evb board is not initialized
- board: Fix ipc_queue example doesn't work on 567-evb as LCPU_RAM_DATA_SIZE is wrong
- board: Update pinmux for eMMC of board sf32lb58-lcd
- board: Fixed PA config of board 567-evb

#### Changed
- board: Rename board `em-lb525` to `sf32lb52-lcd_n16r8`
- board: Change default LCD of sf32lb52-lcd board to LCD_1P85_390*450_DevKit_Adapter_V1.0.0 
- board: Change board `sf32lb52-lcd` touch pin to PA31 to support SF32LB52-DevKit-LCD v1.2 
- board: Change lcd backlight PWM to PWM2 on sf32lb52-lcd board
- board: Rename board `em-lb587` to `sf32lb58-lcd_n16r64n4`
- board: Change board sf32lb56-lcd HCPU default console uart to uart4
- rgbled: Update rgbled driver
- board: Update pc board for simulator build
- board: Optimize LCDC SoftSPI pin IO implementation of 56x-hdk board
- aw8155: move control pin config to kconfig

#### Added
- board: Add dummy board for board independent library build
- sensor: Add LTR303 LSM6DSL MMC56x3 Sensor
- lcd/tp: Added driver for LCD_1P85_390*450_DevKit_Adapter including CO5300 lcd driver and FT6146 touch driver
- board: Add board `sf32lb58-lcd_a128r32n1_dsi`, `sf32lb58-lcd_n16r32n1_dpi`, `sf32lb58-lcd_n16r32n1_dsi`
- board: Add board `sf32lb56-lcd_a128r12n1` and `sf32lb56-lcd_n16r12n1`
- board: Add board `sf32lb52-nano_52b` and `sf32lb52-nano_52j`
- board: Add board `sf32lb52-lchspi-ulp`
- board: Add board `sf32lb52-lcd_52d`
- lcd: Add jdi387a driver
