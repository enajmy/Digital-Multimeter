#include "main.h"
#include "ADC.h"
#include "delay.h"
#include <math.h>

/* -- Global Variables for ADC Conv. Process -- */
static uint8_t ADC_EOC_Flag = 0;
static uint16_t ADCData = 0;

/* -- ADC Initialization Function -- */
void ADC_init(void){

	// Turn on ADC Clocks using HCLK (AHB)
		RCC->AHB2ENR |= RCC_AHB2ENR_ADCEN;
		ADC123_COMMON->CCR = (1 << ADC_CCR_CKMODE_Pos);

		// Power up the ADC and voltage regulator
		ADC1->CR &= ~(ADC_CR_DEEPPWD);
		ADC1->CR |= (ADC_CR_ADVREGEN);

		// Wait 20us for the voltage regulator to startup
		delay_us(20);

		// Configure single ended mode for channel 5
		ADC1->DIFSEL &= ~(ADC_DIFSEL_DIFSEL_5);

		// Calibrate ADC - ensure ADEN is 0 and single ended mode
		ADC1->CR &= ~(ADC_CR_ADEN | ADC_CR_ADCALDIF);
		ADC1->CR |= ADC_CR_ADCAL;

		// Wait for ADCAL to be 0
		while (ADC1->CR & ADC_CR_ADCAL);

		// Enable ADC
		// Clear ADRDY bit by writing 1
		ADC1->ISR |= (ADC_ISR_ADRDY);
		ADC1->CR |= ADC_CR_ADEN;

		// Wait for ADRDY to be 1
		while(!(ADC1->ISR & ADC_ISR_ADRDY));

		// Clear the ADRDY bit by writing 1
		ADC1->ISR |= ADC_ISR_ADRDY;

		// Configure SQR for regular sequence
		ADC1->SQR1 = (5 << ADC_SQR1_SQ1_Pos);

		// Configure channel 5 for sampling time (SMP) with 2.5 clocks
		ADC1->SMPR1 = (0b000 << ADC_SMPR1_SMP5_Pos);
		// # determines sampling rate in register reference page
		// SMP5 is GPIO A0 which corresponds with ADCINN5

		// Configure conversion resolution (RES) 12-bit, right align
		// Single Conversion, software trigger
		ADC1->CFGR = 0;

		/* ---- Enable interrupts ---- */
		// End of conversion interrupt enable
		ADC1->IER |= ADC_IER_EOCIE;

		// Clear the flag
		ADC1->ISR |= (ADC_ISR_EOC);

		// Enable interrupts in NVIC
		NVIC->ISER[0] |= (1 << (ADC1_2_IRQn & 0x1F));

		// Enable interrupts globally
		// ** DISABLED HERE DUE TO ENABLE IN MAIN **
		//__enable_irq();

		/* ---- GPIO Configure ---- */
		// Configure GPIO for PA0 pin for analog mode
		GPIOA->MODER &= ~(GPIO_MODER_MODE0);
		GPIOA->MODER |= (GPIO_MODER_MODE0);

		// Low Speed
		GPIOA->PUPDR &= ~(GPIO_PUPDR_PUPD0);
}

void ADCConvert(uint32_t *voltageValues, uint8_t state){
	uint32_t min = 0xFFFFFFFF, max = 0,
				sum = 0, mean = 0,
				peakpeak = 0;
	double rms = 0;
	uint32_t voltageArray[samples];


	for(int i = 0; i < (samples); i++)
	{
		// Enable CCR1 interrupts
		TIM2->DIER |= (TIM_DIER_CC1IE);

		// Wait until interrupt occurs
		while (!(TIM2->SR & TIM_SR_CC1IF));

		//Start a conversion
		ADC1->CR |= ADC_CR_ADSTART;

		//Wait for the flag set by a conversion finishing
		while(!(ADC_EOC_Flag));

		//Store the converted value in the array
		voltageArray[i] = ADCData;

		//Clear the conversion finished flag
		ADC_EOC_Flag = 0;
	}

	// Disable CCR1 interrupts
	TIM2->DIER &= ~(TIM_DIER_CC1IE);

	// Find the min, max and mean of the samples
	for(int i = 0; i < samples; i++){
		min = voltageArray[i] < min ? voltageArray[i] : min;
		max = voltageArray[i] > max ? voltageArray[i] : max;
		sum += voltageArray[i];
	}

	// If state = 1 (ST_AC), calc RMS
	if (state == 1){
	// Calculate the RMS value
	for(int i = 0; i < samples; i++){

		// Square the element in the array
		voltageArray[i] = pow(voltageArray[i],2);

		// Add squared element to rms
		rms += voltageArray[i];
	}
	// Divide squared elements by # of array val
	rms /= samples;
	// Take square root
	rms = sqrt(rms);
	rms = ((rms * 812) - 1314) / 10000;
	voltageValues[4] = rms;
	}

	// Calculate the mean
	mean = (sum / samples);

	// Convert the values to voltages
	// Turn the 12 bit binary number into a voltage in uV
	// Use calibration equation to get voltage value
	// Then divide by 10000 to get value in 10s of mV
	min = ((min * 812) - 1314) / 10000;
	max = ((max * 812) - 1314) / 10000;
	mean = ((mean * 812) - 1314) / 10000;

	// Prevents min & mean from going very high if negative
	// voltage measured due to min being unsigned.
	if (min > 4000)
		min = 0;
	if (mean > 4000)
		mean = 0;

	// Calculate peak-to-peak from max and min
	peakpeak = max - min;

	// Save min, max, and mean values to array
	voltageValues[0] = min;
	voltageValues[1] = max;
	voltageValues[2] = mean;
	voltageValues[3] = peakpeak;

}

/* -- Function to convert the ADC voltage into a string -- */
// This function was implemented by A7 partner Colt Whitley.
void ADCVoltageToString(char *string, uint32_t value)
{
	// In this application string is always a character array with 4 digits
	// The second element we want to always set to '.'
	for(int i = 0; i < 4; i++)
	{
		// Create and store a character for the ith digit of the number in
		if(i != 2) {
			string[(3) - i] = 0x30 + (value % 10);
			// Shift value over 1 decade
			value /= 10;
		}
		// If we are on the 2nd i we want to add a decimal place
		else string[(3) - i] = '.';
	}
}

void ADC1_2_IRQHandler(void){
	if(ADC1->ISR & ADC_ISR_EOC)
	{
		//Read data into the static variable
		ADCData = ADC1->DR;
		//Set the static flag
		ADC_EOC_Flag = 1;
	}
}
