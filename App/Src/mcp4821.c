#include "mcp4821.h"
#include "spi.h"

#define MCP4821_COMMAND_ACTIVE    0x3000U
#define MCP4821_SPI_TIMEOUT       10U

#define MCP4821_CS_LOW()   HAL_GPIO_WritePin(MCP4821_CS_GPIO_Port, MCP4821_CS_Pin, GPIO_PIN_RESET)
#define MCP4821_CS_HIGH()  HAL_GPIO_WritePin(MCP4821_CS_GPIO_Port, MCP4821_CS_Pin, GPIO_PIN_SET)

HAL_StatusTypeDef MCP4821_Write(uint16_t value)
{
    uint16_t command;
    HAL_StatusTypeDef status;

    command = MCP4821_COMMAND_ACTIVE | (value & 0x0FFFU);
    MCP4821_CS_LOW();
    status = HAL_SPI_Transmit(&hspi2, (uint8_t *)&command, 1U, MCP4821_SPI_TIMEOUT);
    MCP4821_CS_HIGH();
    return status;
}

HAL_StatusTypeDef MCP4821_OutputMidpoint(void)
{
    return MCP4821_Write(MCP4821_MIDPOINT_CODE);
}

HAL_StatusTypeDef MCP4821_Init(void)
{
    MCP4821_CS_HIGH();
    HAL_Delay(1U);
    return MCP4821_OutputMidpoint();
}
