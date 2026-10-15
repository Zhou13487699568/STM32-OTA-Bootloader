#ifndef __BOOT_H
#define __BOOT_H

#include "stm32f1xx_hal.h"

void bootloder_Brance(void);
void BootLoader_Clear(void);
typedef void (*load_a)(void);
void LOAD_A(uint32_t addr);
 void MSR_sp(uint32_t ADDR);
 uint8_t BootLoader_Enter(uint8_t timeout);
 void BootLoader_Info(void);
void bootloder_Event(uint8_t* data,uint8_t datalen);
uint16_t Xmodem_CRC16(uint8_t *data, uint16_t datalen);
/* 功能 6：把 W25Q64 里暂存的固件搬进 A 区并运行。
 * 返回 0 = 搬运成功并已跳转（正常情况不会返回）；
 * 返回 1 = 失败（长度/向量表非法或校验不过），调用者应回到菜单。 */
uint8_t BootLoader_UpdateAFromW25Q64(void);
#endif
