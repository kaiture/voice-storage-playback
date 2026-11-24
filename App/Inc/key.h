#ifndef __KEY_H
#define __KEY_H

#include "main.h"

typedef enum
{
    KEY_NONE = 0,
    KEY1_PRESSED,
    KEY2_PRESSED,
    KEY3_PRESSED,
    KEY4_PRESSED
} KeyTypeDef;

void KEY_Init(void);
KeyTypeDef KEY_Scan(void);

#endif
