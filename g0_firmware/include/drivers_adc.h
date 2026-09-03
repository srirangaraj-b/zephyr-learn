#ifndef ADC_H
#define ADC_H

#include <stdint.h>
#include <float.h>

/**
 * ADC Driver Module
 *
 * Provides abstraction for reading analog voltage from ADC channels.
 * Includes support for voltage and current measurement modes.
 *
 * Hardware facts:
 *  - 6 channels total
 *  - 12-bit resolution → 0-4095 codes
 *  - 0-5V input range
 *  - CH1, CH2: configurable voltage/current (via external jumper)
 *  - CH3-6: voltage only
 *
 * Current measurement:
 *  - Requires I-to-V conversion circuit on channel input
 *  - 4 mA → 0V, 20 mA → 5V (linear)
 *  - Firmware converts 0-5V reading to 4-20 mA range
 */

/**
 * Initialize ADC subsystem
 * For mock: Seeds random generator or loads simulated values
 * For real hardware: Configures ADC registers, calibration, channels
 */
void ADC_Init(void);

/**
 * Read raw ADC code from a channel (12-bit, 0-4095)
 *
 * @param channel Channel number (0-5)
 * @return Raw ADC code (0-4095), or 0 on error
 */
uint16_t ADC_ReadRaw(uint8_t channel);

/**
 * Read voltage from an ADC channel (engineering units: Volts)
 *
 * Converts raw ADC code to voltage: 0-5V range
 *
 * @param channel Channel number (0-5)
 * @return Voltage in Volts (0.0 - 5.0), or 0.0 on error
 */
float ADC_ReadVoltage(uint8_t channel);

/**
 * Read current from an ADC channel configured for current measurement (mA)
 *
 * For channels configured as current inputs:
 *  - Hardware: I-to-V circuit converts 4-20 mA to 0-5V
 *  - Firmware: Converts 0-5V ADC reading to 4-20 mA
 *
 * Formula: I(mA) = 4 + (V_adc / 5) * 16
 *  - 0V ADC → 4 mA
 *  - 2.5V ADC → 12 mA
 *  - 5V ADC → 20 mA
 *
 * @param channel Channel number (0-5, typically 0 or 1)
 * @return Current in mA (4.0 - 20.0), or 0.0 on error
 */
float ADC_ReadCurrent(uint8_t channel);

/**
 * Set mock ADC value for testing (PC only)
 *
 * Allows simulating different input values without real hardware.
 *
 * @param channel Channel number (0-5)
 * @param voltage Voltage to simulate (0.0 - 5.0)
 */
void ADC_SetMockValue(uint8_t channel, float voltage);

#endif // ADC_H
