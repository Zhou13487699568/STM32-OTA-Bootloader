//#ifndef __MY_IIC_H__
//#define __MY_IIC_H__
//#include "stm32f1xx_hal.h"
////===================== 修改成你板子引脚：PB6=SCL，PB7=SDA =====================
//#define I2C_SCL_PIN        GPIO_PIN_6
//#define I2C_SCL_PORT       GPIOB
//#define I2C_SDA_PIN        GPIO_PIN_7
//#define I2C_SDA_PORT       GPIOB

//#define I2C_SCL_H()        HAL_GPIO_WritePin(I2C_SCL_PORT, I2C_SCL_PIN, GPIO_PIN_SET)
//#define I2C_SCL_L()        HAL_GPIO_WritePin(I2C_SCL_PORT, I2C_SCL_PIN, GPIO_PIN_RESET)

//#define I2C_SDA_H()        HAL_GPIO_WritePin(I2C_SDA_PORT, I2C_SDA_PIN, GPIO_PIN_SET)
//#define I2C_SDA_L()        HAL_GPIO_WritePin(I2C_SDA_PORT, I2C_SDA_PIN, GPIO_PIN_RESET)

////读取SDA电平
//#define I2C_SDA_READ()     HAL_GPIO_ReadPin(I2C_SDA_PORT, I2C_SDA_PIN)

//#define BIT(n) (1U << (n))

//void SoftI2C_GPIO_Init(void);
//void SoftI2C_Start(void);
//void SoftI2C_Stop(void);
//void my_iic_sendbyte(uint8_t txd);
//uint8_t IIC_Wait_ACK(int16_t timeout);
//uint8_t my_iic_readbyte(uint8_t ack);

//#endif

#ifndef __MY_IIC_H__
#define __MY_IIC_H__
#include "stm32f1xx_hal.h"

#define I2C_SCL_PIN        GPIO_PIN_6
#define I2C_SCL_PORT       GPIOB
#define I2C_SDA_PIN        GPIO_PIN_7
#define I2C_SDA_PORT       GPIOB

#define I2C_SCL_H()        HAL_GPIO_WritePin(I2C_SCL_PORT, I2C_SCL_PIN, GPIO_PIN_SET)
#define I2C_SCL_L()        HAL_GPIO_WritePin(I2C_SCL_PORT, I2C_SCL_PIN, GPIO_PIN_RESET)
#define I2C_SDA_H()        HAL_GPIO_WritePin(I2C_SDA_PORT, I2C_SDA_PIN, GPIO_PIN_SET)
#define I2C_SDA_L()        HAL_GPIO_WritePin(I2C_SDA_PORT, I2C_SDA_PIN, GPIO_PIN_RESET)
#define I2C_SDA_READ()     HAL_GPIO_ReadPin(I2C_SDA_PORT, I2C_SDA_PIN)

void SoftI2C_GPIO_Init(void);
void SDA_To_Output(void);
void SDA_To_Input(void);
void SoftI2C_Start(void);
void SoftI2C_Stop(void);
void my_iic_sendbyte(uint8_t txd);
uint8_t IIC_Wait_ACK(int16_t timeout);
uint8_t my_iic_readbyte(uint8_t ack);

#endif
