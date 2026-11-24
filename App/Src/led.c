#include "led.h"

static uint8_t led_output = 0xFFU;

static void LED_Write(uint8_t on)
{
    if (led_output != on)
    {
        HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, (on != 0U) ? GPIO_PIN_RESET : GPIO_PIN_SET);
        led_output = on;
    }
}

void LED_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();
    HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
    GPIO_InitStruct.Pin = LED_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_GPIO_Port, &GPIO_InitStruct);
    led_output = 0U;
}

void LED_Control(SystemState state)
{
    if (state == ST_ERROR)
    {
        LED_Write((uint8_t)((HAL_GetTick() / 125U) & 1U));
        return;
    }
    LED_Write(((state == ST_RECORD) || (state == ST_PLAY)) ? 1U : 0U);
}
