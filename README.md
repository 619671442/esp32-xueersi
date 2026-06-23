# 学而思编程掌机 — 自定义固件开发

## 设备概况

学而思编程掌机是 **meow32（喵先生）** 系列，ESP32 + MicroPython 架构。

| 项目 | 信息 | 状态 |
|------|------|------|
| 产品 | 学而思编程掌机（黄色，无触摸屏） | ✅ |
| 主控 | ESP32-WROVER-B（双核 240MHz + 8MB PSRAM） | ✅ |
| USB 串口 | COM3 | ✅ |
| 充电 IC | IP4056（锂电池充电管理） | ✅ |
| 六轴传感器 | MPU-6050A（I2C 地址 0x68） | ✅ |
| 电机驱动 | HR8833（双 H 桥） | ✅ |
| 屏幕 | ST7735 1.8" TFT, 160x128, SPI | ✅ |
| 按键 | 上下左右 + A/B + Reset | ✅ |
| 蜂鸣器 | GPIO 14 | ✅ |
| 固件版本 | V2.0424 / SDK v20220406 | ✅ |
| 扩展接口 | GPIO 25/26/32/33, SCL/SDA, TX/RX, M1/M2 | ✅ |

## 完整引脚映射

来源：MicroPython REPL 直接读取

| 功能 | GPIO | 方向 | 备注 |
|------|------|------|------|
| UP ↑ | **2** | 输入(上拉) |  |
| DOWN ↓ | **13** | 输入(上拉) |  |
| LEFT ← | **27** | 输入(上拉) |  |
| RIGHT → | **35** | 输入(仅输入) |  |
| A (暂停) | **34** | 输入(仅输入) |  |
| B (继续) | **12** | 输入(上拉) |  |
| TFT_CS | **5** | 输出 | `meowbit.cs = Pin(5)` |
| TFT_DC | **4** | 输出 | `meowbit.dc = Pin(4)` |
| TFT_MOSI | **23** | 输出 | VSPI, `meowbit.vspi.mosi=23` |
| TFT_SCLK | **18** | 输出 | VSPI, `meowbit.vspi.sck=18` |
| TFT_MISO | **19** | 输入 | VSPI, `meowbit.vspi.miso=19` |
| TFT_RST | 无 | — | 依赖上电复位 |
| TFT_BL | 硬接常亮 | — | 无法 GPIO 控制 |
| BEEP | **14** | PWM 输出 |  |
| LIGHT_SENSOR | **36** | ADC 输入 |  |
| TEMP_SENSOR | **39** | ADC 输入 |  |
| I2C SCL | **21** | 双向 | MPU-6050 |
| I2C SDA | **22** | 双向 | MPU-6050 |
| 扩展 IO | 25, 26, 32, 33 | 双向 |  |
| SD_CS | 19 | 输出 | 与 MISO 复用 |
| SD_MOSI | 5 | 输出 | 与 CS 复用 |
| SD_SCK | 21 | 输出 | 与 I2C SCL 复用 |
| SD_MISO | 22 | 输入 | 与 I2C SDA 复用 |

注意：显示屏引脚与 SD 卡引脚存在复用，显示时 SD 不可用。

## 目录结构

```
F:\code\xueersi\
├── README.md
├── detect_pins.py         # MicroPython 引脚探测脚本
├── query_repl.py          # 串口 REPL 查询脚本
├── find_mpy.py            # 固件分析工具
└── firmware\
    ├── 01_backup\          # 原厂固件备份 (4MB)
    │   ├── flash_original.bin
    │   ├── meow32_0424_trigfix.bin
    │   ├── backup.bat
    │   └── backup.ps1
    ├── main_firmware\      # 贪吃蛇固件
    │   ├── platformio.ini
    │   └── src\main.cpp
    ├── build.bat           # 编译
    ├── upload.bat          # 上传
    ├── flash.bat           # 编译+上传
    └── firmware.ps1        # PowerShell 版
```

## TFT_eSPI 配置（User_Setup.h）

```cpp
#define ST7735_DRIVER

#define TFT_WIDTH  128
#define TFT_HEIGHT 160

#define TFT_CS   5
#define TFT_DC   4
#define TFT_RST  -1       // 无软件复位
#define TFT_BL   -1       // 背光硬接常亮

#define TFT_MOSI 23
#define TFT_SCLK 18
#define TFT_MISO 19

#define SPI_FREQUENCY  40000000
```

注意：TFT_eSPI 在此硬件上工作不正常，请使用直接 SPI 驱动方式。

## 使用方式

```bash
# 编译
cd firmware
.\build.bat

# 上传（需先关闭编程助手释放 COM3）
.\upload.bat
```

## 开发环境

- **IDE**: VSCode + PlatformIO
- **框架**: Arduino-ESP32
- **屏幕库**: 直接 SPI（不使用 TFT_eSPI）