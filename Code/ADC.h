#ifndef SRC_ADC_H_
#define SRC_ADC_H_

// Defines
#define samples 20000

// Function Declarations
void ADC_init(void);
void ADCConvert(uint32_t *, uint8_t);
void ADCVoltageToString(char *, uint32_t);


#endif /* SRC_ADC_H_ */
