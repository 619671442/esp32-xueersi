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

## 引脚映射

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
| **SD_CS** | **22** | 输出 | 与 I2C SDA 冲突 ⚠️ |
| BEEP | **14** | PWM 输出 | 蜂鸣器 |
| LIGHT_SENSOR | **36** | ADC | 光敏 |
| TEMP_SENSOR | **39** | ADC | 温度 |
| I2C SCL | **21** | 双向 | MPU-6050 |
| I2C SDA | **22** | 双向 | 与 SD_CS 复用 ⚠️ |

> ⚠️ **GPIO 22 冲突**：SD 卡 CS 与 I2C SDA（MPU-6050）共用同一引脚。

## 固件架构

### 原生 SPI 驱动（无 LVGL）

移除 LVGL 图形库依赖，直接操作 SPI 刷新屏幕，代码体积小、性能高。

### 模块化结构

```
firmware/main_firmware/src/
├── main.h              # 全局常量（颜色、引脚定义、WiFi 状态）
├── main.cpp            # 入口：setup + loop 调度
├── font_8x16.h         # 8x16 ASCII 字库
├── input/
│   ├── input.h
│   └── input.cpp       # 按键输入处理
├── lcd/
│   ├── lcd.h
│   └── lcd.cpp         # ST7735 SPI 驱动 + 绘图 API
├── menu/
│   ├── menu.h
│   └── menu.cpp        # 菜单系统（MenuItem 结构体数组）
├── apps/
│   ├── bt_gamepad.*    # 蓝牙手柄（BLE HID）
│   ├── wifi_manager.*  # WiFi 连接管理
│   ├── sd_manager.*    # SD 卡文件浏览器
│   └── webserver.*     # WebServer 管理页面
└── webserver/
    ├── webserver.h
    └── webserver.cpp   # 网页服务器（SD 管理 + WiFi 密码管理）
```

### 菜单循环

菜单通过 `MenuItem` 结构体数组定义，每个项包含 `label`、`init` 函数指针和 `loop` 函数指针。支持滚动显示（最多 3 项可见，共 4 项）。

- ↑↓ 循环切换，**A 确认进入**，**B 返回菜单**
- 右上角 WiFi 状态图标（灰=关闭 / 蓝=热点 / 绿=已连接）

## 应用列表

### 1. BT Gamepad — 蓝牙手柄

将掌机模拟为 BLE HID 游戏手柄，可连接电脑/手机。

- 方向键 → 键盘方向键（UP/DOWN/LEFT/RIGHT）
- A/B → 键盘 A/B
- **LEFT+RIGHT 长按 1 秒**：切换 A/B 键模式（A→Space, B→Enter）

### 2. WiFi Manager — WiFi 管理

- **开启/关闭热点**：ESP32 作为 AP（SSID: ESP32-WIFI）
- **加入网络**：扫描附近 WiFi，选择并连接（密码通过 WebServer 预先保存）

### 3. SD File Manager — SD 卡文件浏览器

- 目录浏览（进入/返回上级）
- 文件列表滚动显示
- 弹出菜单：删除文件/目录、查看文件详情（名称/类型/大小）、退出

### 4. WebServer — 网页管理

通过浏览器访问 ESP32 的 Web 页面：

- **SD Card Manager**：浏览/上传/下载/删除 SD 卡文件，创建目录
- **WiFi Password Manager**：保存/忘记 WiFi 密码（用于 WiFi Manager 应用）

## 开发环境

- **IDE**: VSCode + PlatformIO
- **框架**: Arduino-ESP32
- **屏幕驱动**: 直接 SPI（CS=5, DC=4, MOSI=23, SCLK=18, 40MHz）

## 使用方式

```powershell
cd firmware

# 编译
.\firmware.ps1 build

# 上传（需先关闭学而思编程助手释放 COM3）
.\firmware.ps1 upload

# 编译 + 上传
.\firmware.ps1 all
```