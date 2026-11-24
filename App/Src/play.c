#include "play.h"
#include "mcp4821.h"
#include "record.h"
#include "tim.h"
#include "w25q64.h"

static uint8_t play_buf[PLAY_BUF_LENGTH];
static volatile uint16_t play_index;
static volatile uint32_t play_position;
static volatile uint8_t half_ready[2];
static volatile uint8_t playing;
static volatile uint8_t paused;
static volatile uint8_t finished;
static volatile uint8_t play_error;
static uint32_t play_length;
static uint32_t read_address;

static HAL_StatusTypeDef Play_RefillHalf(uint8_t half)
{
    uint32_t offset = (uint32_t)half * PLAY_HALF_LENGTH;
    uint32_t remaining = (read_address < play_length) ? (play_length - read_address) : 0U;
    uint16_t chunk = (remaining > PLAY_HALF_LENGTH) ? PLAY_HALF_LENGTH : (uint16_t)remaining;

    if (chunk != 0U)
    {
        if (W25Q64_Read(read_address, &play_buf[offset], chunk) != HAL_OK)
        {
            return HAL_ERROR;
        }
        read_address += chunk;
    }
    __DMB();
    half_ready[half] = 1U;
    return HAL_OK;
}

HAL_StatusTypeDef Play_Init(void)
{
    playing = 0U;
    paused = 0U;
    finished = 0U;
    play_error = 0U;
    play_index = 0U;
    play_position = 0U;
    read_address = 0U;
    play_length = 0U;
    half_ready[0] = 0U;
    half_ready[1] = 0U;
    return MCP4821_OutputMidpoint();
}

HAL_StatusTypeDef Play_Start(void)
{
    uint32_t initial_length;
    HAL_StatusTypeDef status;

    if ((Record_IsActive() != 0U) || (playing != 0U))
    {
        return HAL_BUSY;
    }

    play_length = Record_GetLength();
    if (play_length == 0U)
    {
        return HAL_ERROR;
    }

    initial_length = (play_length > PLAY_BUF_LENGTH) ? PLAY_BUF_LENGTH : play_length;
    if (W25Q64_Read(0U, play_buf, initial_length) != HAL_OK)
    {
        return HAL_ERROR;
    }

    read_address = initial_length;
    play_index = 0U;
    play_position = 0U;
    half_ready[0] = 1U;
    half_ready[1] = 1U;
    finished = 0U;
    play_error = 0U;
    paused = 0U;
    playing = 1U;

    __HAL_TIM_SET_COUNTER(&htim2, 0U);
    __HAL_TIM_CLEAR_FLAG(&htim2, TIM_FLAG_UPDATE);
    status = HAL_TIM_Base_Start_IT(&htim2);
    if (status != HAL_OK)
    {
        playing = 0U;
        return status;
    }
    return HAL_OK;
}

HAL_StatusTypeDef Play_Pause(void)
{
    if ((playing == 0U) || (paused != 0U) || (finished != 0U))
    {
        return HAL_ERROR;
    }
    if (HAL_TIM_Base_Stop_IT(&htim2) != HAL_OK)
    {
        play_error = 1U;
        return HAL_ERROR;
    }
    paused = 1U;
    return HAL_OK;
}

HAL_StatusTypeDef Play_Resume(void)
{
    HAL_StatusTypeDef status;

    if ((playing == 0U) || (paused == 0U) || (finished != 0U))
    {
        return HAL_ERROR;
    }

    if (Play_Process() != HAL_OK)
    {
        play_error = 1U;
        return HAL_ERROR;
    }
    __HAL_TIM_CLEAR_FLAG(&htim2, TIM_FLAG_UPDATE);
    paused = 0U;
    status = HAL_TIM_Base_Start_IT(&htim2);
    if (status != HAL_OK)
    {
        paused = 1U;
        play_error = 1U;
    }
    return status;
}

HAL_StatusTypeDef Play_Stop(void)
{
    HAL_StatusTypeDef status;

    status = HAL_TIM_Base_Stop_IT(&htim2);
    playing = 0U;
    paused = 0U;
    finished = 0U;
    play_error = 0U;
    play_index = 0U;
    play_position = 0U;
    half_ready[0] = 0U;
    half_ready[1] = 0U;
    if (MCP4821_OutputMidpoint() != HAL_OK)
    {
        status = HAL_ERROR;
    }
    return status;
}

HAL_StatusTypeDef Play_Process(void)
{
    if (play_error != 0U)
    {
        return HAL_ERROR;
    }
    if ((playing == 0U) || (finished != 0U))
    {
        return HAL_OK;
    }

    if ((half_ready[0] == 0U) && (Play_RefillHalf(0U) != HAL_OK))
    {
        return HAL_ERROR;
    }
    if ((half_ready[1] == 0U) && (Play_RefillHalf(1U) != HAL_OK))
    {
        return HAL_ERROR;
    }
    return HAL_OK;
}

uint8_t Play_IsActive(void)
{
    return playing;
}

uint8_t Play_IsFinished(void)
{
    return finished;
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    uint8_t sample;

    if (htim->Instance != TIM2)
    {
        return;
    }

    if ((playing == 0U) || (paused != 0U))
    {
        return;
    }
    if (play_position >= play_length)
    {
        playing = 0U;
        finished = 1U;
        return;
    }

    sample = play_buf[play_index];
    if (MCP4821_Write((uint16_t)sample << 4) != HAL_OK)
    {
        play_error = 1U;
        playing = 0U;
        return;
    }
    play_position++;
    play_index++;

    if (play_position < play_length)
    {
        uint8_t next_half;

        if (play_index == PLAY_HALF_LENGTH)
        {
            next_half = 1U;
        }
        else if (play_index == PLAY_BUF_LENGTH)
        {
            play_index = 0U;
            next_half = 0U;
        }
        else
        {
            return;
        }

        /* 仅切换到主循环已经填充完毕的缓冲。 */
        if (half_ready[next_half] == 0U)
        {
            play_error = 1U;
            playing = 0U;
            return;
        }
        __DMB();
        half_ready[next_half ^ 1U] = 0U;
    }
}
