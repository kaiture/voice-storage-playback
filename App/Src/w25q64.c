#include "w25q64.h"
#include "spi.h"
#include <stddef.h>

#define W25Q64_CMD_WRITE_ENABLE     0x06U
#define W25Q64_CMD_READ_STATUS1     0x05U
#define W25Q64_CMD_PAGE_PROGRAM     0x02U
#define W25Q64_CMD_READ_DATA        0x03U
#define W25Q64_CMD_SECTOR_ERASE     0x20U
#define W25Q64_CMD_JEDEC_ID         0x9FU
#define W25Q64_CMD_RELEASE_POWERDOWN 0xABU

#define W25Q64_STATUS_BUSY          0x01U
#define W25Q64_SPI_TIMEOUT          1000U
#define W25Q64_BUSY_TIMEOUT         3000U

static uint32_t flash_size = 0U;
static uint32_t erase_start_tick;
uint32_t W25Q64_DebugID = 0U;

#define W25Q64_CS_LOW()   HAL_GPIO_WritePin(W25Q64_CS_GPIO_Port, W25Q64_CS_Pin, GPIO_PIN_RESET)
#define W25Q64_CS_HIGH()  HAL_GPIO_WritePin(W25Q64_CS_GPIO_Port, W25Q64_CS_Pin, GPIO_PIN_SET)

static HAL_StatusTypeDef W25Q64_SendCommand(uint8_t command)
{
    HAL_StatusTypeDef status;

    W25Q64_CS_LOW();
    status = HAL_SPI_Transmit(&hspi1, &command, 1U, W25Q64_SPI_TIMEOUT);
    W25Q64_CS_HIGH();
    return status;
}

static HAL_StatusTypeDef W25Q64_ReadStatus(uint8_t *status_value)
{
    uint8_t command = W25Q64_CMD_READ_STATUS1;
    HAL_StatusTypeDef status;

    W25Q64_CS_LOW();
    status = HAL_SPI_Transmit(&hspi1, &command, 1U, W25Q64_SPI_TIMEOUT);
    if (status == HAL_OK)
    {
        status = HAL_SPI_Receive(&hspi1, status_value, 1U, W25Q64_SPI_TIMEOUT);
    }
    W25Q64_CS_HIGH();
    return status;
}

static HAL_StatusTypeDef W25Q64_WaitBusy(void)
{
    uint32_t start_tick = HAL_GetTick();
    uint8_t status_value;

    do
    {
        if (W25Q64_ReadStatus(&status_value) != HAL_OK)
        {
            return HAL_ERROR;
        }
        if ((status_value & W25Q64_STATUS_BUSY) == 0U)
        {
            return HAL_OK;
        }
        HAL_Delay(1U);
    } while ((HAL_GetTick() - start_tick) < W25Q64_BUSY_TIMEOUT);

    return HAL_TIMEOUT;
}

static HAL_StatusTypeDef W25Q64_WriteEnable(void)
{
    return W25Q64_SendCommand(W25Q64_CMD_WRITE_ENABLE);
}

HAL_StatusTypeDef W25Q64_ReadID(uint32_t *id)
{
    uint8_t command = W25Q64_CMD_JEDEC_ID;
    uint8_t id_bytes[3];
    HAL_StatusTypeDef status;

    if (id == NULL)
    {
        return HAL_ERROR;
    }

    W25Q64_CS_LOW();
    status = HAL_SPI_Transmit(&hspi1, &command, 1U, W25Q64_SPI_TIMEOUT);
    if (status == HAL_OK)
    {
        status = HAL_SPI_Receive(&hspi1, id_bytes, 3U, W25Q64_SPI_TIMEOUT);
    }
    W25Q64_CS_HIGH();

    if (status == HAL_OK)
    {
        *id = ((uint32_t)id_bytes[0] << 16) |
              ((uint32_t)id_bytes[1] << 8) |
              (uint32_t)id_bytes[2];
    }
    return status;
}

uint32_t W25Q64_GetSize(void)
{
    return flash_size;
}

HAL_StatusTypeDef W25Q64_Init(void)
{
    uint32_t id = 0U;
    HAL_StatusTypeDef status;

    W25Q64_CS_HIGH();
    HAL_Delay(1U);
    status = W25Q64_SendCommand(W25Q64_CMD_RELEASE_POWERDOWN);
    if (status != HAL_OK)
    {
        return status;
    }
    HAL_Delay(1U);

    status = W25Q64_ReadID(&id);
    if (status != HAL_OK)
    {
        return status;
    }
    W25Q64_DebugID = id;   /* 无论校验是否通过, 记录原始 ID 供诊断 */

    /* 按 W25Q 系列识别: 任意厂商兼容芯片, 类型字节 0x40, 容量由密度字节决定 */
    if (((id >> 8) & 0xFFUL) != 0x40UL)
    {
        return HAL_ERROR;
    }
    switch (id & 0xFFUL)
    {
        case 0x14UL: flash_size = 1UL << 20; break;   /* 1MB  (W25Q80)  */
        case 0x15UL: flash_size = 2UL << 20; break;   /* 2MB  (W25Q16)  */
        case 0x16UL: flash_size = 4UL << 20; break;   /* 4MB  (W25Q32)  */
        case 0x17UL: flash_size = 8UL << 20; break;   /* 8MB  (W25Q64)  */
        case 0x18UL: flash_size = 16UL << 20; break;  /* 16MB (W25Q128) */
        default: return HAL_ERROR;
    }
    return W25Q64_WaitBusy();
}

HAL_StatusTypeDef W25Q64_Read(uint32_t address, uint8_t *data, uint32_t length)
{
    uint8_t header[4];
    HAL_StatusTypeDef status = HAL_OK;

    if (((data == NULL) && (length != 0U)) ||
        (address >= flash_size) ||
        (length > (flash_size - address)))
    {
        return HAL_ERROR;
    }
    if (length == 0U)
    {
        return HAL_OK;
    }

    header[0] = W25Q64_CMD_READ_DATA;
    header[1] = (uint8_t)(address >> 16);
    header[2] = (uint8_t)(address >> 8);
    header[3] = (uint8_t)address;

    W25Q64_CS_LOW();
    status = HAL_SPI_Transmit(&hspi1, header, 4U, W25Q64_SPI_TIMEOUT);
    while ((status == HAL_OK) && (length != 0U))
    {
        uint16_t chunk = (length > 65535UL) ? 65535U : (uint16_t)length;

        status = HAL_SPI_Receive(&hspi1, data, chunk, W25Q64_SPI_TIMEOUT);
        data += chunk;
        length -= chunk;
    }
    W25Q64_CS_HIGH();
    return status;
}

HAL_StatusTypeDef W25Q64_Write(uint32_t address, const uint8_t *data, uint32_t length)
{
    if (((data == NULL) && (length != 0U)) ||
        (address >= flash_size) ||
        (length > (flash_size - address)))
    {
        return HAL_ERROR;
    }

    while (length != 0U)
    {
        uint8_t header[4];
        uint32_t page_remaining = W25Q64_PAGE_SIZE - (address % W25Q64_PAGE_SIZE);
        uint16_t chunk = (length > page_remaining) ? (uint16_t)page_remaining : (uint16_t)length;
        HAL_StatusTypeDef status;

        if (W25Q64_WaitBusy() != HAL_OK)
        {
            return HAL_ERROR;
        }
        if (W25Q64_WriteEnable() != HAL_OK)
        {
            return HAL_ERROR;
        }

        header[0] = W25Q64_CMD_PAGE_PROGRAM;
        header[1] = (uint8_t)(address >> 16);
        header[2] = (uint8_t)(address >> 8);
        header[3] = (uint8_t)address;

        W25Q64_CS_LOW();
        status = HAL_SPI_Transmit(&hspi1, header, 4U, W25Q64_SPI_TIMEOUT);
        if (status == HAL_OK)
        {
            status = HAL_SPI_Transmit(&hspi1, (uint8_t *)data, chunk, W25Q64_SPI_TIMEOUT);
        }
        W25Q64_CS_HIGH();
        if (status != HAL_OK)
        {
            return status;
        }
        if (W25Q64_WaitBusy() != HAL_OK)
        {
            return HAL_ERROR;
        }

        address += chunk;
        data += chunk;
        length -= chunk;
    }
    return HAL_OK;
}

HAL_StatusTypeDef W25Q64_BeginSectorErase(uint32_t address)
{
    uint8_t command[4];
    HAL_StatusTypeDef status;

    if (address >= flash_size)
    {
        return HAL_ERROR;
    }
    address &= ~(W25Q64_SECTOR_SIZE - 1UL);

    if (W25Q64_WaitBusy() != HAL_OK)
    {
        return HAL_ERROR;
    }
    if (W25Q64_WriteEnable() != HAL_OK)
    {
        return HAL_ERROR;
    }

    command[0] = W25Q64_CMD_SECTOR_ERASE;
    command[1] = (uint8_t)(address >> 16);
    command[2] = (uint8_t)(address >> 8);
    command[3] = (uint8_t)address;

    W25Q64_CS_LOW();
    status = HAL_SPI_Transmit(&hspi1, command, 4U, W25Q64_SPI_TIMEOUT);
    W25Q64_CS_HIGH();
    if (status != HAL_OK)
    {
        return status;
    }
    erase_start_tick = HAL_GetTick();
    return HAL_OK;
}

HAL_StatusTypeDef W25Q64_PollErase(void)
{
    uint8_t status_value;
    HAL_StatusTypeDef status = W25Q64_ReadStatus(&status_value);

    if (status != HAL_OK)
    {
        return status;
    }
    if ((status_value & W25Q64_STATUS_BUSY) == 0U)
    {
        return HAL_OK;
    }
    if ((HAL_GetTick() - erase_start_tick) >= W25Q64_BUSY_TIMEOUT)
    {
        return HAL_TIMEOUT;
    }
    return HAL_BUSY;
}

HAL_StatusTypeDef W25Q64_EraseSector(uint32_t address)
{
    HAL_StatusTypeDef status = W25Q64_BeginSectorErase(address);

    if (status != HAL_OK)
    {
        return status;
    }
    do
    {
        status = W25Q64_PollErase();
        if (status == HAL_BUSY)
        {
            HAL_Delay(1U);
        }
    } while (status == HAL_BUSY);

    return status;
}
