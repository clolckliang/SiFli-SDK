# I2C SLAVE示例
源码路径：example/hal/i2c/slave

## 支持的平台
例程可以运行在以下开发板.
* sf32lb52-lcd_n16r8
* sf32lb58-lcd_n16r64n4
* sf32lb56-lcd_n16r12n1

## 概述
* 本示例演示了如何使用芯片的I2C接口实现主从通信功能。代码实现了I2C从机模式(EXAMPLE_I2C_SLAVE)，在此模式下：
1. 设备地址设置为0x5A
2. 初始化I2C接口为从机模式
3. 接收主机发送的4字节数据
4. 向主机发送预设的4字节数据{0xAA, 0xBB, 0xCC, 0xDD}


## 工作原理
* 在从机模式下，设备会循环执行以下操作：
1. 等待接收主机发送的数据（阻塞等待）
2. 成功接收到数据后，打印接收到的数据
3. 延迟100毫秒
4. 向主机发送预设的应答数据
5. 打印传输状态

## 例程的使用
* 以sf32lb52-lcd_n16r8开发板为例，运行本例程，查看串口输出。

### 硬件需求
* 运行该例程前，需要准备：
+ 一块本例程支持的开发板
+ 杜邦线若干

### 硬件连接
两块开发板I2C连接方式如下：
|开发板    |SDA管脚|SDA管脚名称|SCL管脚|SCL管脚名称|
|:---     |:---    |:---     |:---   |:---      |
|sf32lb52-lcd_n16r8 |3       |PA42     |5      |PA41      |
|sf32lb56-lcd_n16r12n1 |3       |PA12     |5      |PA20      |
|sf32lb58-lcd_n16r64n4 |3 (CONN1)   |PB29     |5 (CONN1)  |PB28      |

### 编译和烧录
切换到例程project目录，运行scons命令执行编译：
```
scons --board=sf32lb52-lcd_n16r8 -j8
```

运行`build_sf32lb52-lcd_n16r8_hcpu\uart_download.bat`，按提示选择端口即可进行下载：

```
build_sf32lb52-lcd_n16r8_hcpu\uart_download.bat

Uart Download

please input the serial port num:5
```

### 例程输出结果展示:
* log输出:
```
11-18 20:11:21:771    SFBL
11-18 20:11:23:829    Serial:c2,Chip:4,Package:3,Rev:2  Reason:00000000
11-18 20:11:23:833     \ | /
11-18 20:11:23:834    - SiFli Corporation
11-18 20:11:23:837     / | \     build on Nov 18 2025, 2.4.0 build dd4cae55
11-18 20:11:23:839     2020 - 2022 Copyright by SiFli team
11-18 20:11:23:840    mount /dev sucess
11-18 20:11:23:841    [I/drv.rtc] PSCLR=0x80000100 DivAI=128 DivAF=0 B=256
11-18 20:11:23:845    [I/drv.rtc] RTC use LXT RTC_CR=00000001
11-18 20:11:23:848    [I/drv.rtc] Init RTC, wake = 0
11-18 20:11:23:849    [I/drv.audprc] init 00 ADC_PATH_CFG0 0x606
11-18 20:11:23:849    [I/drv.audprc] HAL_AUDPRC_Init res 0
11-18 20:11:23:852    [I/drv.audcodec] HAL_AUDCODEC_Init res 0
11-18 20:11:23:853    [32m[I/TOUCH] Regist touch screen driver, probe=1203bec5 [0m
11-18 20:11:23:854    call par CFG1(3313)
11-18 20:11:23:855    fc 9, xtal 2000, pll 2047
11-18 20:11:23:855    call par CFG1(3313)
11-18 20:11:23:856    fc 9, xtal 2000, pll 2046
11-18 20:11:23:865    Start I2C Slave Demo!
11-18 20:11:23:867    I2C Slave Init Success!
11-18 20:11:23:876    msh />
11-18 20:11:24:342    Slave Receive Success, data: 0x11  0x22  0x33  0x44  
11-18 20:11:24:443    Slave Transmit Success!
11-18 20:11:24:944    Slave Receive Success, data: 0x11  0x22  0x33  0x44  
11-18 20:11:25:044    Slave Transmit Success!
11-18 20:11:25:544    Slave Receive Success, data: 0x11  0x22  0x33  0x44  
11-18 20:11:25:643    Slave Transmit Success!
11-18 20:11:26:144    Slave Receive Success, data: 0x11  0x22  0x33  0x44  
11-18 20:11:26:244    Slave Transmit Success!
11-18 20:11:26:744    Slave Receive Success, data: 0x11  0x22  0x33  0x44  
11-18 20:11:26:844    Slave Transmit Success!
11-18 20:11:27:344    Slave Receive Success, data: 0x11  0x22  0x33  0x44  
11-18 20:11:27:444    Slave Transmit Success!
11-18 20:11:27:944    Slave Receive Success, data: 0x11  0x22  0x33  0x44  
11-18 20:11:28:044    Slave Transmit Success!
11-18 20:11:28:545    Slave Receive Success, data: 0x11  0x22  0x33  0x44  
11-18 20:11:28:645    Slave Transmit Success!
11-18 20:11:29:145    Slave Receive Success, data: 0x11  0x22  0x33  0x44  
11-18 20:11:29:245    Slave Transmit Success!
11-18 20:11:29:744    Slave Receive Success, data: 0x11  0x22  0x33  0x44  
```
### I2C读写波形
#### 逻辑分析仪抓取部分波形
* i2c slave发送（i2c master读）的波形
![alt text](assets/i2c_slave_send.png)
* i2c slave接收（i2c master写）的波形
![alt text](assets/i2c_slave_receive.png)

#### I2C参数修改
* 见I2C_Slave_init函数内注释
```c
void I2C_Slave_Init(void)
{
    HAL_StatusTypeDef ret;
    //pin nux
#ifdef SF32LB52X
    HAL_RCC_EnableModule(RCC_MOD_I2C2); // enable i2c2
    HAL_PIN_Set(PAD_PA41, I2C2_SCL, PIN_PULLUP, 1); // i2c io select
    HAL_PIN_Set(PAD_PA42, I2C2_SDA, PIN_PULLUP, 1);
#define SLAVE_I2C I2C2// i2c number of cpu
#elif defined(SF32LB58X)
    HAL_RCC_EnableModule(RCC_MOD_I2C6);  // enable i2c6
    HAL_PIN_Set(PAD_PB28, I2C6_SCL, PIN_PULLUP, 0); // i2c io select
    HAL_PIN_Set(PAD_PB29, I2C6_SDA, PIN_PULLUP, 0);
#define SLAVE_I2C I2C6// i2c number of cpu
#elif defined(SF32LB56X)
    HAL_RCC_EnableModule(RCC_MOD_I2C3); // enable i2c3
    HAL_PIN_Set(PAD_PA20, I2C3_SCL, PIN_PULLUP, 1); // i2c io select
    HAL_PIN_Set(PAD_PA12, I2C3_SDA, PIN_PULLUP, 1);
#define SLAVE_I2C I2C3// i2c number of cpu
#endif
    //i2c init
    hi2c.Instance = SLAVE_I2C;// i2c slave mode
    hi2c.Mode = HAL_I2C_MODE_SLAVE;
    hi2c.Init.ClockSpeed = 400000;           // i2c speed(hz)
    hi2c.Init.OwnAddress1 = SLAVE_ADDRESS;  // slave address
    hi2c.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;// i2c 7bits device address mode
    hi2c.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;

    ret = HAL_I2C_Init(&hi2c);
    if (ret != HAL_OK) {
        rt_kprintf("I2C Init failed: %d\n", ret);
        return;
    }
    rt_kprintf("I2C Slave Init Success!\n");
}
```
* 依据芯片类型区分开发板，在初始化函数中，针对特定芯片，配置与之对应的I2C引脚
* 例如通过`#elif defined(SF32LB52X)`<br>芯片来进行一个判断使用的是哪个开发板
* 通过`#define EXAMPLE_I2C I2C2`<br>为该芯片使用的I2C控制器编号（比如I2C6、I2C3）
* 最后再通过HAL_PIN_Set()函数配置I2C的SCL和SDA引脚，并且需要设置为上拉模式

**注意：** 
1. 除55x芯片外,可以配置到任意带有PA*_I2C_UART功能的IO输出I2C的SDA,SCLK波形
2.  HAL_PIN_Set 最后一个参数为hcpu/lcpu选择, 1:选择hcpu,0:选择lcpu 

## 异常诊断
* I2C无波形输出
1. 对照芯片手册检查CPU的I2C是否选择正确
2. 检查IO配置和连接是否正确
* 从机一直处于等待状态
1. 检查I2C从机地址是否正确
2. 检查I2C从机地址是否被其他设备占用
