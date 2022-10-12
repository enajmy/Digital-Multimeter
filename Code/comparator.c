/* ---- comparator.c ---- */
#include "main.h"
#include "comparator.h"
#include <string.h>

void Comparator_init(void){
	/* -- GPIOB Enable -- */
		// Turn on GPIOB Clock
		RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN;

		// Set AFR to AF12 for use of PB0 as comparator out
		COMPAR1->AFR[0] &= ~(GPIO_AFRL_AFSEL0);
		COMPAR1->AFR[0] |= (0b1100 << GPIO_AFRL_AFSEL0_Pos);

		// Set PB0 to AF Mode for comparator output
		COMPAR1->MODER &= ~(GPIO_MODER_MODE0);
		COMPAR1->MODER |= (0b10 << GPIO_MODER_MODE0_Pos);

		// Set PB1 and PB2 to Analog Mode
		COMPAR1->MODER &= ~(GPIO_MODER_MODE1
				| GPIO_MODER_MODE2);
		COMPAR1->MODER |= ((0b11 << GPIO_MODER_MODE1_Pos)
				| (0b11 << GPIO_MODER_MODE2_Pos));

		// Push-Pull
		COMPAR1->OTYPER &= ~(GPIO_OTYPER_OT0
				| GPIO_OTYPER_OT1
				| GPIO_OTYPER_OT2);

		// Low Speeds
		COMPAR1->OSPEEDR &= ~(GPIO_OSPEEDR_OSPEED0
			| GPIO_OSPEEDR_OSPEED1
			| GPIO_OSPEEDR_OSPEED2);

		// No PUPD
		COMPAR1->PUPDR &= ~(GPIO_PUPDR_PUPD0
				| GPIO_PUPDR_PUPD1
				| GPIO_PUPDR_PUPD2);

		/* -- Comparator Enable -- */
		// Configure Input+ of comparator for PB2
		COMP1->CSR &= ~(COMP_CSR_INPSEL);
		COMP1->CSR |= (0b1 << COMP_CSR_INPSEL_Pos);

		// Configure Input- of comparator for PB1
		COMP1->CSR &= ~(COMP_CSR_INMSEL);
		COMP1->CSR |= (0b110 << COMP_CSR_INMSEL_Pos);

		// Enable Hysteresis for Noise Reduction
		COMP1->CSR |= (0b11 << COMP_CSR_HYST_Pos);

		/* -- Timer 2 CCR4 Enable -- */
		// Connect CCR4 to COMP1_OUT
		TIM2->OR1 |= (0b1 << TIM2_OR1_TI4_RMP_Pos);

		// Set Capture/Compare Selection
		TIM2->CCMR2 |= (0b1 << TIM_CCMR2_CC4S_Pos);

		// Set Capture Buffer to prevent double counting period
		TIM2->CCMR2 |= (0b11 << TIM_CCMR2_IC4F_Pos);

		// Set rising edge
		TIM2->CCER &= ~(TIM_CCER_CC4NP);
		TIM2->CCER &= ~(TIM_CCER_CC4P);

		// Enable capture mode
		TIM2->CCER &= ~(TIM_CCER_CC4E);
		TIM2->CCER |= (0b1 << TIM_CCER_CC4E_Pos);

		// Enable Capture/Compare Generation
		TIM2->EGR &= ~(TIM_EGR_CC4G);
		TIM2->EGR |= (0b1 << TIM_EGR_CC4G_Pos);
}


// Used to convert digits to strings for UART
void FreqToString(char *string, uint32_t value){

	// for the length of the string
	for(int i = 0; i < strlen(string); i++){

		// add hex 30 to the value and save in last array spot
		string[((strlen(string) - 1) - i)] = 0x30 + (value % 10);

		// divide value by 10 to add to next array slot
		value /= 10;
	}
}
