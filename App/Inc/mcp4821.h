#ifndef __MCP4821_H
#define __MCP4821_H

#include "main.h"

#define MCP4821_MIDPOINT_CODE    2048U

HAL_StatusTypeDef MCP4821_Init(void);
HAL_StatusTypeDef MCP4821_Write(uint16_t value);
HAL_StatusTypeDef MCP4821_OutputMidpoint(void);

#endif
