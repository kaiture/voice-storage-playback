#ifndef __RECORD_H
#define __RECORD_H

#include "main.h"
#include "w25q64.h"

#define AUDIO_SAMPLE_RATE         8000UL
#define RECORD_OVERSAMPLE         4U                        /* ADC 以 32kHz 采样, 每 4 个样本平均成 1 个 */
#define ADC_BUF_LENGTH            (2U * RECORD_BLOCK_SIZE * RECORD_OVERSAMPLE) /* 原始样本缓冲: 半块 = 256 输出样本 */
#define RECORD_BLOCK_SIZE         256U
#define RECORD_QUEUE_BLOCKS       32U
/* 元数据存放在 flash 最后一个扇区, 音频数据区容量 = 实际芯片容量 - 1 个扇区(运行时检测) */

HAL_StatusTypeDef Record_Init(void);
HAL_StatusTypeDef Record_Start(void);
HAL_StatusTypeDef Record_Stop(void);
HAL_StatusTypeDef Record_Pause(void);
HAL_StatusTypeDef Record_Resume(void);
HAL_StatusTypeDef Record_Process(void);

uint8_t Record_IsActive(void);
uint8_t Record_IsPaused(void);
uint8_t Record_IsFull(void);
uint8_t Record_HasError(void);
uint32_t Record_GetLength(void);

#endif
