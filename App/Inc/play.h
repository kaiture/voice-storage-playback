#ifndef __PLAY_H
#define __PLAY_H

#include "main.h"

#define PLAY_BUF_LENGTH       512U
#define PLAY_HALF_LENGTH      256U

HAL_StatusTypeDef Play_Init(void);
HAL_StatusTypeDef Play_Start(void);
HAL_StatusTypeDef Play_Stop(void);
HAL_StatusTypeDef Play_Pause(void);
HAL_StatusTypeDef Play_Resume(void);
HAL_StatusTypeDef Play_Process(void);
uint8_t Play_IsActive(void);
uint8_t Play_IsFinished(void);

#endif
