# 构建说明

## PC（macOS / Linux）

依赖：CMake 3.20+、C++20 编译器、OpenSSL（macOS 上可通过 Homebrew 安装）、SDL2（仅 host_sim）。

```bash
cd /Volumes/aigo_1t/Github/MultiAIQuota

# 仅 CLI / core / tests
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

# 包含 LVGL 主机模拟器（需要 SDL2）
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DBUILD_HOST_SIM=ON
cmake --build build -j

# 运行测试
./build/tests/maiq_core_tests

# CLI 使用示例
./build/cli/maiq init > maiq.json
# 编辑 maiq.json 填入真实 api_key
./build/cli/maiq query
./build/cli/maiq query --format json
./build/cli/maiq list

# 运行主机模拟器
./build/host_sim/maiq_gui_host
```

## Web 前端

前端使用 Svelte + Vite，源码在 `hw_monitor/frontend/`。构建产物输出到 `hw_monitor/www/`，并由 `scripts/pack_www.mjs` 打包为 `hw_monitor/www.tar.gz`（ustar + gzip level 9，可被 macOS Keka 与 ESP32 ROM miniz 解压），随固件嵌入。

```bash
cd /Volumes/aigo_1t/Github/MultiAIQuota/hw_monitor/frontend
npm install
npm run build        # vite build + 打包 www.tar.gz
```

每次修改前端后都需要重新 `npm run build`，再重新构建并烧录 ESP32 固件。

## ESP32（IDF 5.5）

依赖：ESP-IDF 5.5 已安装并导出环境变量，Node.js/npm（构建前端）。

```bash
cd /Volumes/aigo_1t/Github/MultiAIQuota
. /Volumes/aigo_1t/DevPkgs/esp/esp-idf_5.5/export.sh

# 1. 构建前端（必须先做，否则 www.tar.gz 不存在）
cd hw_monitor/frontend
npm install
npm run build
cd ..

# 2. 参考 Xueersi 板为经典 ESP32（ESP32-WROVER，4 MB flash，8 MB PSRAM）
idf.py set-target esp32
idf.py build

# 3. 首次烧录：分区表已变更（ota_0 + ota_1，无 factory/LittleFS），需全量擦除
idf.py -p /dev/cu.usbserial-xxx erase-flash
idf.py -p /dev/cu.usbserial-xxx flash monitor
```

`idf.py monitor` 默认使用 2000000 bps 的串口波特率（见 `sdkconfig.defaults` 中的 `CONFIG_ESP_CONSOLE_UART_BAUDRATE`）。

### 固件 OTA（压缩固件）

固件支持通过 Web 页面的「升级」标签页上传压缩固件（`build/custom_ota_binaries/MultiAIQuotaMonitor.bin.xz.packed`），设备重启后由自定义 bootloader 解压并切换分区。

```bash
# 压缩固件在每次 idf.py build 时自动生成；也可单独执行
idf.py gen_compressed_ota
```

产物 `build/custom_ota_binaries/MultiAIQuotaMonitor.bin.xz.packed`（约 900KB，小于 ota_1 分区 1.25MB）。

> 注意：压缩升级不可回滚（bootloader_support_plus 双分区方案限制），升级前请确认压缩固件可用。

### 关键配置

- 分区表：`hw_monitor/partitions.csv`（4 MB flash）：`nvs` + `otadata` + `phy_init` + `ota_0`(2.5MB 标准固件) + `ota_1`(1.25MB 压缩固件)，**无 factory/LittleFS 分区**。
- 自定义 bootloader：`hw_monitor/bootloader_components/`（基于 IDF `bootloader_start.c`，集成 `espressif/bootloader_support_plus`，启动时解压压缩固件到 ota_0）。
- `sdkconfig.defaults`：通用选项（flash、CPU、C++ 异常/RTTI、自定义分区表、LVGL、SPIRAM）。
- `sdkconfig.defaults.esp32`：Xueersi 板默认配置。
- `sdkconfig.defaults.esp32s3`：ESP32-S3 默认配置（启用 SPIRAM）。
- 电源管理：已启用 `CONFIG_PM_ENABLE` 与 `CONFIG_FREERTOS_USE_TICKLESS_IDLE`，并在 `app.cpp` 中调用 `esp_pm_configure()` 开启 automatic light sleep。
- Web 静态资源：`hw_monitor/components/www_store/` 组件——`www.tar.gz` 通过 `EMBED_FILES` 嵌入固件，启动时用 ESP32 ROM miniz（tinfl，堆分配缓冲）解压到 PSRAM 并建立内存文件索引，`web_server` 直接读取，无 flash 文件系统。
- 组件依赖：`hw_monitor/main/idf_component.yml` 依赖 `espressif/button`、`espressif/bootloader_support_plus`、`espressif/cmake_utilities`，由组件管理器自动下载。
- LVGL 配置：`third_party/lv_conf.h`（ESP32，16-bit 颜色深度）。该文件位于 `lvgl` 目录之外，避免修改 `third_party/lvgl` 子仓库。

### 第三方组件

`third_party/` 已包含：

- `ArduinoJson/`（v7.3.0）
- `lvgl/`（v9.2.2）
通过 `hw_monitor/CMakeLists.txt` 中的 `EXTRA_COMPONENT_DIRS` 引入。

## 硬件与网络说明

当前已根据 `claude-desktop-buddy-esp32` 中的 Xueersi 板实现以下组件：

- `board`：GPIO 初始化、LCD 硬件复位、按键上拉、蜂鸣器引脚默认低电平。
- `display`：ST7735 160×128 SPI 驱动，软件 CS/DC，10 MHz，横屏 MADCTL=0xA0。
- `input`：两个独立按钮输入设备（KEY1 GPIO34、KEY2 GPIO12，均低电平有效）。KEY1 切换当前选中账户，KEY2 立即触发刷新。使用 `espressif/button` 组件进行硬件消抖，并启用 `enable_power_save` 支持 light sleep 唤醒。
- `wifi`：STA + SmartConfig（ESPTouch），NVS 持久化存储 SSID/密码。
- `web_server`：内置 HTTP 服务器，提供 RESTful JSON API、Svelte 前端与固件 OTA。

### RESTful API

| 方法 | 路径 | 说明 |
|---|---|---|
| GET | `/api/health` | 设备存活 |
| GET | `/api/screenshot` | 获取当前屏幕 BMP 截图 |
| GET | `/api/config` | 获取账户配置 JSON（密钥已脱敏） |
| POST | `/api/config` | 保存账户配置 JSON |
| GET | `/api/wifi/status` | Wi-Fi 状态 |
| POST | `/api/wifi/connect` | 连接指定 Wi-Fi `{ssid,password}` |
| POST | `/api/wifi/clear` | 清除 Wi-Fi 凭据并断开 |
| POST | `/api/query` | 使用当前配置触发查询 |
| POST | `/api/ota` | 上传压缩固件（`.bin.xz.packed`）并重启升级 |

访问设备 IP 即加载前端页面。

主机模拟器使用 160×128 横屏布局（参考 Xueersi），GUI 页面代码与硬件共用。
