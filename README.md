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
| SD 卡槽 | SPI 模式，与屏幕同总线 |

## 引脚映射（已确认）

来源：MicroPython REPL + 实测 + SD 卡测试固件扫描

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
| TFT_MOSI | **23** | 输出 | VSPI MOSI（与 SD 共用） |
| TFT_SCLK | **18** | 输出 | VSPI SCK（与 SD 共用） |
| TFT_MISO | **19** | 输入 | VSPI MISO（与 SD 共用） |
| TFT_RST | 无 | — | 依赖上电复位 |
| TFT_BL | 硬接常亮 | — | 无法 GPIO 控制 |
| **SD_CS** | **22** | 输出 | ✅ 实测，与 I2C SDA 冲突 ⚠️ |
| BEEP | **14** | PWM 输出 | 蜂鸣器 |
| LIGHT_SENSOR | **36** | ADC | 光敏 |
| TEMP_SENSOR | **39** | ADC | 温度 |
| I2C SCL | **21** | 双向 | MPU-6050 |
| I2C SDA | **22** | 双向 | MPU-6050（⚠️ 与 SD_CS 复用）|

> ⚠️ **GPIO 22 冲突**：SD 卡 CS 与 I2C SDA（MPU-6050）共用同一引脚，两者不可同时使用。如使用 SD 卡需停用 I2C，反之亦然。

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

## SD 卡检测结果

通过 `test_firmware` 扫描固件实测确认：

| 信号 | 引脚 | 备注 |
|------|------|------|
| SD_MOSI | **GPIO 23** | 与 TFT_MOSI 共用（VSPI 总线） |
| SD_MISO | **GPIO 19** | 与 TFT_MISO 共用（VSPI 总线） |
| SD_SCLK | **GPIO 18** | 与 TFT_SCLK 共用（VSPI 总线） |
| **SD_CS** | **GPIO 22** | ✅ **实测确认**，与 I2C SDA（MPU-6050）冲突 |

**SPI 总线拓扑：**

```
ESP32 VSPI ─┬─ TFT (CS=5, DC=4)
            └─ SD 卡 (CS=22)
```

- MOSI/MISO/SCLK 三线共享，通过不同 CS 引脚分时选通
- 两设备不可同时通信，但切换只需操作 CS 引脚，无需重新初始化 SPI

## NES 模拟器集成方案

### 可行性

**可以。** 核心思路：SD 卡仅在加载 ROM 时使用，运行时数据全在 PSRAM 中。

| 条件 | 状态 |
|------|------|
| PSRAM 8MB | ✅ NES ROM 通常 8KB~512KB，完全装得下 |
| SPI 分时复用 | ✅ SD 与屏幕同总线，CS 不同，切换高效 |
| ESP32 算力 | ✅ 240MHz 双核，跑 NES 模拟器绰绰有余 |
| 屏幕分辨率 | ⚠️ 160×128 较低，需裁剪 NES 输出（原始 256×240） |
| 按键 | ✅ 6 键可直接对应 NES 方向键 + A/B/Select/Start |
| 开源模拟器 | ✅ 有 ESP32 移植的 NES 模拟器参考（如 `nesemu`、`InfoNES`）|

### 推荐架构

```
┌─ 用户选择 NES App ──────────────────────────────┐
│                                                    │
│  1. 暂停 LVGL 刷屏                                 │
│  2. TFT_CS(LOW) 拉高 → 释放 SPI 总线               │
│  3. SPI 降频到 400kHz（SD 初始化需要）               │
│  4. SD.begin(22) → 挂载 SD 卡                      │
│  5. 用户选择 .nes 文件（LVGL 文件列表）              │
│  6. SD 升频到 20MHz → fread ROM 到 PSRAM            │
│  7. SD.end() → TFT_CS 拉低 → 恢复显示               │
│  8. SPI 升回 40MHz → 开始模拟                      │
│  9. NES 模拟器直接从 PSRAM 读取 ROM，输出到屏幕      │
│                                                    │
└────────────────────────────────────────────────────┘
```

### 实现路线

| 步骤 | 内容 | 依赖 |
|------|------|------|
| 1 | 主固件中集成 SD 卡初始化 + 文件列表显示 | 本文 SD 检测结果 |
| 2 | 移植轻量 NES 模拟器核心到 ESP32（PSRAM 运行） | 开源模拟器 |
| 3 | 模拟器输出到 160×128 屏幕（缩放/裁剪） | LVGL canvas 或直接 SPI |
| 4 | 按键映射（↑↓←→A/B → NES 控制器） | 现有按键代码 |
| 5 | 蜂鸣器音频输出（GPIO 14 PWM） | 可选 |

```
F:\code\xueersi\
└── firmware\
    ├── firmware.ps1           # 主固件编译/上传脚本
    ├── main_firmware\          # LVGL 菜单固件
    │   ├── platformio.ini
    │   ├── lv_conf.h           # LVGL 配置（16位色深）
    │   └── src\main.cpp        # 全部代码（SPI驱动+LVGL+菜单）
    ├── test_firmware\          # SD 卡引脚扫描测试固件
    │   ├── platformio.ini
    │   └── src\main.cpp        # SD 卡 pin 扫描程序
    └── test.ps1                # 测试固件编译/上传/串口监视脚本
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