# 学而思编程掌机 — 自定义固件开发

## 设备概况

学而思编程掌机是 **meow32（喵先生）** 系列，ESP32 + MicroPython 架构。

| 项目 | 信息 |
|------|------|
| 产品 | 学而思编程掌机（黄色，无触摸屏） |
| 主控 | ESP32-WROVER-B（双核 240MHz + 8MB PSRAM） |
| USB 串口 | COM3 |
| 充电 IC | IP4056（锂电池充电管理） |
| 六轴传感器 | MPU-6050A（I2C 地址 0x68） |
| 电机驱动 | HR8833（双 H 桥） |
| 屏幕 | ST7735 1.8" TFT, 160x128, SPI, 14 针 FPC |
| 按键 | 上下左右 + A/B + Reset（6 个） |
| 蜂鸣器 | GPIO 14 |
| 固件版本 | V2.0424 / SDK v20220406 |
| 扩展接口 | GPIO 25/26/32/33, SCL/SDA, TX/RX, M1/M2 |

## 引脚映射（已确认）

来源：MicroPython REPL + 实测

| 功能 | GPIO | 方向 | 备注 |
|------|------|------|------|
| UP ↑ | **2** | 输入(上拉) | |
| DOWN ↓ | **13** | 输入(上拉) | |
| LEFT ← | **27** | 输入(上拉) | |
| RIGHT → | **35** | 输入(仅输入) | |
| A | **34** | 输入(仅输入) | 确认/进入 |
| B | **12** | 输入(上拉) | 取消/返回 |
| TFT_CS | **5** | 输出 | VSPI 片选 |
| TFT_DC | **4** | 输出 | 数据/命令 |
| TFT_MOSI | **23** | 输出 | VSPI MOSI |
| TFT_SCLK | **18** | 输出 | VSPI SCK |
| TFT_MISO | **19** | 输入 | VSPI MISO |
| TFT_RST | 无 | — | 依赖上电复位 |
| TFT_BL | 硬接常亮 | — | 无法 GPIO 控制 |
| BEEP | **14** | PWM 输出 | 蜂鸣器 |
| LIGHT_SENSOR | **36** | ADC | 光敏 |
| TEMP_SENSOR | **39** | ADC | 温度 |
| I2C SCL | **21** | 双向 | MPU-6050 |
| I2C SDA | **22** | 双向 | MPU-6050 |

注意：显示屏 CS(=5)、SCLK(=18)、MISO(=19) 与 SD 卡引脚复用，显示时 SD 不可用。

## 当前固件：LVGL 菜单系统

### 功能

- **SELECT APP 菜单** — 三个功能入口（Snake / BtPad / NES）
- ↑↓ 切换选中（蓝色高亮），**A 确认进入**，**B 返回菜单**
- LVGL 软件旋转（`LV_DISP_ROT_270`）适配物理屏幕方向
- 手动 GPIO 读取（不依赖 LVGL indev 驱动）

### 添加新功能

在 `main.cpp` 的 `enter_app()` 中添加新的页面即可：

```cpp
void enter_app(int i) {
  in_app = true;
  lv_obj_clean(lv_scr_act());
  lv_obj_t* page = lv_label_create(lv_scr_act());
  switch (i) {
    case 0: lv_label_set_text(page, "Snake Game"); break;
    case 1: lv_label_set_text(page, "Bluetooth"); break;
    case 2: lv_label_set_text(page, "NES Emulator"); break;
    // 新增: case 3: ...
  }
  lv_obj_center(page);
  app_label = page;
}
```

并在菜单列表 `names[]` 和按钮创建循环中同步添加。

## 目录结构

```
F:\code\xueersi\
├── README.md
├── detect_pins.py          # MicroPython 探测脚本（存档）
└── firmware\
    ├── 01_backup\           # 原厂固件备份
    │   ├── flash_original.bin
    │   ├── meow32_0424_trigfix.bin
    │   ├── backup.bat
    │   └── backup.ps1
    ├── main_firmware\       # LVGL 菜单固件
    │   ├── platformio.ini
    │   ├── lv_conf.h        # LVGL 配置（16位色深）
    │   └── src\main.cpp     # 全部代码（SPI驱动+LVGL+菜单）
    ├── build.bat            # 编译
    ├── upload.bat           # 上传（需关闭编程助手）
    └── flash.bat            # 编译+上传
```

## 开发环境

- **IDE**: VSCode + PlatformIO
- **框架**: Arduino-ESP32
- **GUI**: LVGL 8.4.0
- **屏幕驱动**: 直接 SPI（CS=5, DC=4, MOSI=23, SCLK=18, 40MHz）
- **LVGL 配置**: 16 位色深, 128x160, `LV_DISP_ROT_270`

## 使用方式

```powershell
cd firmware

# 编译
.\build.bat

# 上传（需先关闭学而思编程助手释放 COM3）
.\upload.bat
```