#include "key.h"

#define KEY_SCAN_PERIOD_MS     10U
#define KEY_DEBOUNCE_SAMPLES   3U

static uint8_t stable_state[4];
static uint8_t debounce_count[4];
static uint32_t last_scan_tick;

static uint8_t KEY_Read(uint8_t index)
{
    GPIO_PinState pin_state = GPIO_PIN_SET;

    switch (index)
    {
        case 0U:
            pin_state = HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin);
            break;
        case 1U:
            pin_state = HAL_GPIO_ReadPin(KEY2_GPIO_Port, KEY2_Pin);
            break;
        case 2U:
            pin_state = HAL_GPIO_ReadPin(KEY3_GPIO_Port, KEY3_Pin);
            break;
        case 3U:
            pin_state = HAL_GPIO_ReadPin(KEY4_GPIO_Port, KEY4_Pin);
            break;
        default:
            break;
    } 
    return (pin_state == GPIO_PIN_RESET) ? 1U : 0U;
}

void KEY_Init(void)
{
    uint8_t i;

    for (i = 0U; i < 4U; ++i)
    {
        stable_state[i] = KEY_Read(i);
        debounce_count[i] = 0U;
    }
    last_scan_tick = HAL_GetTick();
}

KeyTypeDef KEY_Scan(void)
{
    uint32_t now = HAL_GetTick();
    uint8_t i;

    if ((now - last_scan_tick) < KEY_SCAN_PERIOD_MS)
    {
        return KEY_NONE;
    }
    last_scan_tick = now;

    for (i = 0U; i < 4U; ++i)
    {
        uint8_t current = KEY_Read(i);

        if (current == stable_state[i])
        {
            debounce_count[i] = 0U;
            continue;
        }

        debounce_count[i]++;
        if (debounce_count[i] < KEY_DEBOUNCE_SAMPLES)
        {
            continue;
        }

        stable_state[i] = current;
        debounce_count[i] = 0U;
        if (current != 0U)
        {
            return (KeyTypeDef)(KEY1_PRESSED + i);
        }
    }
    return KEY_NONE;
}
