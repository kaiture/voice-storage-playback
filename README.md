# STM32 语音存储与回放

基于 STM32F103C8T6 的板载录音与回放系统。TIM3 以 32 kHz 触发 ADC 采样，DMA 搬运采样数据。固件将每 4 个样本平均为 1 个，保存为 8 kHz、8 位 PCM，写入 W25Q64 SPI Flash。回放时固件从 Flash 读取音频，通过 MCP4821 DAC 输出。

## 实板演示

[查看或下载实板演示（MP4，约 49 秒）](docs/hardware-demo.mp4)

## 按键操作

| 按键 | 操作 |
| --- | --- |
| KEY1 | 开始或停止录音 |
| KEY2 | 暂停或继续当前录音或回放 |
| KEY3 | 开始或停止回放 |
| KEY4 | 从头播放已保存的录音 |

## 编译

使用 Keil MDK 打开 [MDK 工程](MDK-ARM/voice-storage-playback.uvprojx)，选择 `voice-storage-playback` 目标并编译。工程配置使用 Arm Compiler 5.06 和 STM32F1 设备支持包。[CubeMX 配置](voice-storage-playback.ioc)可用于查看引脚与外设设置。

应用代码位于 `App/`，STM32 初始化和主循环位于 `Core/`，HAL 与 CMSIS 位于 `Drivers/`。

## 制作教程

克隆或下载仓库后，在浏览器中打开 [制作教程](docs/tutorial/index.html)，查看接线、原理图和 PCB 制作步骤。
