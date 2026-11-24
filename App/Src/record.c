#include "record.h"
#include "adc.h"
#include "tim.h"
#include <stddef.h>

#define AUDIO_METADATA_MAGIC      0x564F4943UL
#define AUDIO_METADATA_CHECK      0xA5A5A5A5UL

typedef struct
{
    uint32_t magic;
    uint32_t length;
    uint32_t sample_rate;
    uint32_t checksum;
} AudioMetadata;

static uint16_t adc_buf[ADC_BUF_LENGTH];
static uint8_t record_queue[RECORD_QUEUE_BLOCKS][RECORD_BLOCK_SIZE];

static volatile uint8_t queue_head;
static volatile uint8_t queue_tail;
static volatile uint8_t accepting_samples;
static volatile uint8_t capture_error;
static volatile uint8_t storage_full;
static volatile uint8_t recording;
static uint8_t paused;
static uint32_t captured_length;  /* 采样中断累计入队的字节数 */
static uint32_t write_address;
static uint32_t next_erase_address;
static uint8_t erase_pending;
static uint32_t erase_poll_tick;
static uint8_t storage_error;
static uint32_t audio_length;
static uint32_t audio_capacity;   /* 音频数据区容量 = flash 容量 - 元数据扇区 */

static HAL_StatusTypeDef Record_LoadMetadata(void)
{
    AudioMetadata metadata;

    audio_length = 0U;
    if (W25Q64_Read(audio_capacity, (uint8_t *)&metadata, sizeof(metadata)) != HAL_OK)
    {
        return HAL_ERROR;
    }

    if ((metadata.magic == AUDIO_METADATA_MAGIC) &&
        (metadata.sample_rate == AUDIO_SAMPLE_RATE) &&
        (metadata.length <= audio_capacity) &&
        (metadata.checksum == (metadata.magic ^ metadata.length ^
                              metadata.sample_rate ^ AUDIO_METADATA_CHECK)))
    {
        audio_length = metadata.length;
    }
    return HAL_OK;
}

static HAL_StatusTypeDef Record_EraseMetadata(void)
{
    AudioMetadata metadata;

    if ((W25Q64_EraseSector(audio_capacity) != HAL_OK) ||
        (W25Q64_Read(audio_capacity, (uint8_t *)&metadata, sizeof(metadata)) != HAL_OK))
    {
        return HAL_ERROR;
    }

    return ((metadata.magic == UINT32_MAX) && (metadata.length == UINT32_MAX) &&
            (metadata.sample_rate == UINT32_MAX) && (metadata.checksum == UINT32_MAX))
           ? HAL_OK : HAL_ERROR;
}

static HAL_StatusTypeDef Record_SaveMetadata(void)
{
    AudioMetadata metadata;
    AudioMetadata stored;
    uint32_t fields_offset = (uint32_t)offsetof(AudioMetadata, length);
    const uint8_t *bytes = (const uint8_t *)&metadata;

    metadata.magic = AUDIO_METADATA_MAGIC;
    metadata.length = write_address;
    metadata.sample_rate = AUDIO_SAMPLE_RATE;
    metadata.checksum = metadata.magic ^ metadata.length ^
                        metadata.sample_rate ^ AUDIO_METADATA_CHECK;

    /* 先写入并核对字段，最后写入 magic 以提交录音。 */
    if ((W25Q64_Write(audio_capacity + fields_offset, bytes + fields_offset,
                     sizeof(metadata) - fields_offset) != HAL_OK) ||
        (W25Q64_Read(audio_capacity, (uint8_t *)&stored, sizeof(stored)) != HAL_OK) ||
        (stored.magic != UINT32_MAX) || (stored.length != metadata.length) ||
        (stored.sample_rate != metadata.sample_rate) || (stored.checksum != metadata.checksum) ||
        (W25Q64_Write(audio_capacity, bytes, sizeof(metadata.magic)) != HAL_OK) ||
        (Record_LoadMetadata() != HAL_OK) || (audio_length != write_address))
    {
        audio_length = 0U;
        return HAL_ERROR;
    }
    return HAL_OK;
}

static void Record_EnqueueBlock(const uint16_t *samples)
{
    uint8_t head;
    uint8_t next_head;
    uint32_t i;

    if ((recording == 0U) || (accepting_samples == 0U))
    {
        return;
    }

    head = queue_head;
    next_head = head + 1U;
    if (next_head >= RECORD_QUEUE_BLOCKS)
    {
        next_head = 0U;
    }
    if (next_head == queue_tail)
    {
        capture_error = 1U;
        accepting_samples = 0U;
        return;
    }

    /* 四个 12 位样本求和后量化；舍入结果可能达到 256。 */
    for (i = 0U; i < RECORD_BLOCK_SIZE; ++i)
    {
        uint32_t sum = (uint32_t)samples[RECORD_OVERSAMPLE * i] +
                       (uint32_t)samples[RECORD_OVERSAMPLE * i + 1U] +
                       (uint32_t)samples[RECORD_OVERSAMPLE * i + 2U] +
                       (uint32_t)samples[RECORD_OVERSAMPLE * i + 3U];
        uint32_t code = (sum + 8U) >> 6U;
        record_queue[head][i] = (uint8_t)((code > 255U) ? 255U : code);
    }
    __DMB();
    queue_head = next_head;
    captured_length += RECORD_BLOCK_SIZE;
    if (captured_length >= audio_capacity)
    {
        storage_full = 1U;
        accepting_samples = 0U;
    }
}

HAL_StatusTypeDef Record_Init(void)
{
    queue_head = 0U;
    queue_tail = 0U;
    recording = 0U;
    paused = 0U;
    accepting_samples = 0U;
    capture_error = 0U;
    storage_full = 0U;
    write_address = 0U;
    next_erase_address = 0U;

    if (W25Q64_GetSize() <= W25Q64_SECTOR_SIZE)
    {
        return HAL_ERROR;
    }
    audio_capacity = W25Q64_GetSize() - W25Q64_SECTOR_SIZE;
    if (Record_LoadMetadata() != HAL_OK)
    {
        return HAL_ERROR;
    }
    return HAL_ADCEx_Calibration_Start(&hadc1);
}

HAL_StatusTypeDef Record_Start(void)
{
    HAL_StatusTypeDef status;

    if (recording != 0U)
    {
        return HAL_BUSY;
    }

    queue_head = 0U;
    queue_tail = 0U;
    write_address = 0U;
    next_erase_address = 0U;
    audio_length = 0U;
    captured_length = 0U;
    erase_pending = 0U;
    storage_error = 0U;
    capture_error = 0U;
    storage_full = 0U;
    accepting_samples = 0U;
    paused = 0U;
    /* 在覆盖任何音频前，先清除旧的有效元数据。 */
    if (Record_EraseMetadata() != HAL_OK)
    {
        return HAL_ERROR;
    }

    recording = 1U;
    accepting_samples = 1U;
    status = HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_buf, ADC_BUF_LENGTH);
    if (status != HAL_OK)
    {
        recording = 0U;
        accepting_samples = 0U;
        return status;
    }

    __HAL_TIM_SET_COUNTER(&htim3, 0U);
    __HAL_TIM_CLEAR_FLAG(&htim3, TIM_FLAG_UPDATE);
    status = HAL_TIM_Base_Start(&htim3);
    if (status != HAL_OK)
    {
        HAL_ADC_Stop_DMA(&hadc1);
        recording = 0U;
        accepting_samples = 0U;
        return status;
    }
    return HAL_OK;
}

HAL_StatusTypeDef Record_Pause(void)
{
    if ((recording == 0U) || (paused != 0U))
    {
        return HAL_ERROR;
    }

    accepting_samples = 0U;
    if (HAL_TIM_Base_Stop(&htim3) != HAL_OK)
    {
        capture_error = 1U;
    }
    if (HAL_ADC_Stop_DMA(&hadc1) != HAL_OK)
    {
        capture_error = 1U;
    }
    paused = 1U;
    return (capture_error != 0U) ? HAL_ERROR : HAL_OK;
}

HAL_StatusTypeDef Record_Resume(void)
{
    HAL_StatusTypeDef status;

    if ((recording == 0U) || (paused == 0U) ||
        (capture_error != 0U) || (storage_full != 0U))
    {
        return HAL_ERROR;
    }

    __HAL_TIM_SET_COUNTER(&htim3, 0U);
    __HAL_TIM_CLEAR_FLAG(&htim3, TIM_FLAG_UPDATE);
    status = HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_buf, ADC_BUF_LENGTH);
    if (status != HAL_OK)
    {
        capture_error = 1U;
        return status;
    }

    paused = 0U;
    accepting_samples = 1U;
    status = HAL_TIM_Base_Start(&htim3);
    if (status != HAL_OK)
    {
        capture_error = 1U;
        accepting_samples = 0U;
        paused = 1U;
        HAL_ADC_Stop_DMA(&hadc1);
        return status;
    }
    return HAL_OK;
}

HAL_StatusTypeDef Record_Process(void)
{
    HAL_StatusTypeDef status;
    uint8_t tail = queue_tail;

    if (storage_error != 0U)
    {
        return HAL_ERROR;
    }
    if (tail == queue_head)
    {
        return HAL_OK;
    }

    if (erase_pending != 0U)
    {
        uint32_t now = HAL_GetTick();

        if (now == erase_poll_tick)
        {
            return HAL_OK;
        }
        erase_poll_tick = now;
        status = W25Q64_PollErase();
        if (status == HAL_BUSY)
        {
            return HAL_OK;
        }
        erase_pending = 0U;
        if (status != HAL_OK)
        {
            storage_error = 1U;
            return status;
        }
        next_erase_address += W25Q64_SECTOR_SIZE;
    }

    if (write_address == next_erase_address)
    {
        status = W25Q64_BeginSectorErase(write_address);
        if (status != HAL_OK)
        {
            storage_error = 1U;
            return status;
        }
        erase_poll_tick = HAL_GetTick();
        erase_pending = 1U;
        return HAL_OK;
    }

    __DMB();
    status = W25Q64_Write(write_address, record_queue[tail], RECORD_BLOCK_SIZE);
    if (status != HAL_OK)
    {
        storage_error = 1U;
        return status;
    }

    write_address += RECORD_BLOCK_SIZE;
    __DMB();
    queue_tail = ((tail + 1U) == RECORD_QUEUE_BLOCKS) ? 0U : (uint8_t)(tail + 1U);
    return HAL_OK;
}

HAL_StatusTypeDef Record_Stop(void)
{
    HAL_StatusTypeDef status = HAL_OK;

    if (recording == 0U)
    {
        return HAL_OK;
    }

    if (paused == 0U)
    {
        Record_Pause();
    }
    recording = 0U;
    paused = 0U;

    /* 即使采集失败，也保存已完整入队的数据。 */
    while ((queue_head != queue_tail) && (storage_error == 0U))
    {
        status = Record_Process();
        if (status != HAL_OK)
        {
            break;
        }
        if (erase_pending != 0U)
        {
            HAL_Delay(1U);
        }
    }
    if (storage_error != 0U)
    {
        return HAL_ERROR;
    }
    if ((write_address != 0U) && (Record_SaveMetadata() != HAL_OK))
    {
        return HAL_ERROR;
    }
    return (capture_error != 0U) ? HAL_ERROR : status;
}

uint8_t Record_IsActive(void)
{
    return recording;
}

uint8_t Record_IsPaused(void)
{
    return paused;
}

uint8_t Record_IsFull(void)
{
    return ((storage_full != 0U) && (queue_head == queue_tail)) ? 1U : 0U;
}

uint8_t Record_HasError(void)
{
    return capture_error;
}

uint32_t Record_GetLength(void)
{
    return audio_length;
}

void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC1)
    {
        Record_EnqueueBlock(&adc_buf[0]);
    }
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC1)
    {
        Record_EnqueueBlock(&adc_buf[ADC_BUF_LENGTH / 2U]);
    }
}

void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *hadc)
{
    if ((hadc->Instance == ADC1) && (recording != 0U))
    {
        accepting_samples = 0U;
        capture_error = 1U;
    }
}
