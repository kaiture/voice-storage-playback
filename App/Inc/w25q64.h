#ifndef __W25Q64_H
#define __W25Q64_H

#include "main.h"

#define W25Q64_SECTOR_SIZE      4096UL
#define W25Q64_PAGE_SIZE        256UL

HAL_StatusTypeDef W25Q64_Init(void);
uint32_t W25Q64_GetSize(void);
extern uint32_t W25Q64_DebugID;   /* 最近一次 Init 读到的 JEDEC ID 原始值(诊断用) */
HAL_StatusTypeDef W25Q64_ReadID(uint32_t *id);
HAL_StatusTypeDef W25Q64_Read(uint32_t address, uint8_t *data, uint32_t length);
HAL_StatusTypeDef W25Q64_Write(uint32_t address, const uint8_t *data, uint32_t length);
HAL_StatusTypeDef W25Q64_BeginSectorErase(uint32_t address);
/* HAL_BUSY: 擦除仍在进行, 完成后才能读写 Flash */
HAL_StatusTypeDef W25Q64_PollErase(void);
HAL_StatusTypeDef W25Q64_EraseSector(uint32_t address);

#endif
