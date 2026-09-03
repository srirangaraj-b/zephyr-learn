#ifndef DAC_H
#define DAC_H

#include <stdint.h>

#define DAC_CHANNEL_A   0x00
#define DAC_CHANNEL_B   0x01
#define DAC_CHANNEL_C   0x02
#define DAC_CHANNEL_D   0x03

void DAC_Init(void);
int DAC_SetRawCode(uint8_t channel, uint16_t code);
int DAC_SetVoltage(uint8_t channel, float voltage);
float DAC_GetVoltage(uint8_t channel);
int DAC_SetCurrent(uint8_t channel, float current_mA);
float DAC_GetCurrent(uint8_t channel);
int DAC_SetSafe(void);
const struct device *DAC_GetDevice(void);

#endif // DAC_H