// Created by: Ethan Najmy
// Project 3 - Digital Multimeter
// CPE 329, Professor Hummel, Spring 2022
// June 6, 2022

#include "main.h"
#include "ADC.h"
#include "delay.h"
#include "LPUART.h"
#include "comparator.h"


#define CLK_SPEED 24000000
#define CCR_Std 120000

void SystemClock_Config(void);

// Global variable to keep track of states in ISR
uint8_t stateVal = 0;

// Global interrupt flag
uint8_t interruptFlag = 0;

// Global change state flag
uint8_t changeStateFlag = 0;

// Used to keep track of LPUART RX data
uint8_t stateChangeValue = 0;

// Used to save CCR rising edge value
uint32_t CCR_Capture = 0, CCR_Capture2 = 0;

// CCR1 initial value
uint32_t CCR_Value = 120000;

// Used to count comparator interrupts
uint8_t compCount = 0;

int main(void){

	/* -- Create FSM -- */
	typedef enum {ST_DC, ST_AC, ST_SEL} State_Type;
	State_Type state = ST_DC;


	/* -- Variables -- */
	uint32_t voltageValues[5];
	char tempVoltageString[] = "0.00";
	char tempFreqString[] = "000";
	uint32_t CCR_Period;
	uint32_t frequency = 0;
	uint8_t serialPrintCount = 0;


	/* -- Initialization -- */
	// Initialize System Functions
	HAL_Init();
	SystemClock_Config();

	// Initialize peripherals
	ADC_init();
	LPUART_init();

	// Turn on timer clock & init. comparator
	RCC->APB1ENR1 |= RCC_APB1ENR1_TIM2EN;
	Comparator_init();

	// Set CCR1 to 120,000 (0.005s)
	// CCR1 will trigger when to begin converting values
	TIM2->CCR1 = CCR_Std;

	// Update interrupt event!!!!
	TIM2->EGR |= TIM_EGR_UG;

	// Enable TIM2 interrupts
	NVIC->ISER[0] |= (1 << (TIM2_IRQn & 0x1F));

	// Enable global interrupts
	__enable_irq();

	// Start timer
	TIM2->CR1 |= TIM_CR1_CEN;

	// Clear Screen
	LPUART_ESC_Code("[2J");

	while (1) {

		// FSM!
		switch(state){

		// Used to switch between waveforms
		case ST_DC:{

			// Check if should change stage
			if (changeStateFlag){
				state = ST_SEL;
				break;
			}

			// Set global state variable for IRQ
			stateVal = 1;

			CCR_Value = CCR_Std;
			// Set CCR1
			TIM2->CCR1 = CCR_Value;
			// Update interrupt event!!!!
			TIM2->EGR |= TIM_EGR_UG;

			// Start ADC Conversion to capture voltages
			// voltageValues[0] = min
			// voltageValues[1] = max
			// voltageValues[2] = mean (Offset)
			// voltageValues[3] = peak-peak
			ADCConvert(voltageValues, state);

			// Clear Screen
			LPUART_ESC_Code("[2J");
			// Return cursor home
			LPUART_ESC_Code("[H");
			// Print
			LPUART_print("DC MODE");
			// Move down 2 lines
			LPUART_ESC_Code("[2E");
			// Print max value
			LPUART_print("Voltage: ");
			ADCVoltageToString(tempVoltageString, voltageValues[2]);
			LPUART_print(tempVoltageString);
			LPUART_print(" V ");

			LPUART_ESC_Code("[2E");

			serialPrintCount = (voltageValues[2] * 12) / 100;

			for (int i = 0; i <= serialPrintCount; i++)
				LPUART_print("#");

			LPUART_ESC_Code("[1E");
			LPUART_print("|-----|-----|-----|-----|-----|-----|");
			LPUART_ESC_Code("[1E");
			LPUART_print("0    0.5   1.0   1.5   2.0   2.5   3.0");
			// Move down 1 line
			LPUART_ESC_Code("[1E");


			/* -- Used to delay outputting/measuring -- */
			// Set IRQ state
			stateVal = 0;
			// Set CCR1
			TIM2->CCR1 = 1200000;
			// Update interrupt event!!!!
			TIM2->EGR |= TIM_EGR_UG;
			// Enable CCR1 interrupts
			TIM2->DIER |= (TIM_DIER_CC1IE);
			// Wait until interrupt occurs
			while (!(TIM2->SR & TIM_SR_CC1IF));
			// Disable CCR1 interrupts
			TIM2->DIER &= ~(TIM_DIER_CC1IE);


			// Check if change state again (speeds up transition)
			if (changeStateFlag){
				state = ST_SEL;
				break;
			}

			break;
		}

		case ST_AC:{

			// Disable CCR1 interrupts
			TIM2->DIER &= ~(TIM_DIER_CC1IE);

			// Check if need to change state
			if (changeStateFlag){
				state = ST_SEL;
				break;
			}

			// Set ISR value for freq. measurements
			stateVal = 2;

			// Enable TIM2 CCR4 Interrupts
			TIM2->DIER |= (TIM_DIER_CC4IE);

			// Wait until 2 comparator cycles have occurred
			// Then we know frequency is ready to measure
			while (compCount != 2);

			// Disable Interrupts for CCR4
			TIM2->DIER &= ~(TIM_DIER_CC4IE);

			// Subtract CCR captures to determine period in cycles
			CCR_Period = (CCR_Capture2 - CCR_Capture) - 150;

			// Reset count
			compCount = 0;

			// Set stateVal = 1 for ISR
			stateVal = 1;

			CCR_Value = CCR_Period / (samples * 10);
			// Want 200 samples / period
			TIM2->CCR1 = CCR_Value;
			// Update interrupt event!!!!
			TIM2->EGR |= TIM_EGR_UG;

			// Start ADC Conversion to capture voltages
			// voltageValues[0] = min
			// voltageValues[1] = max
			// voltageValues[2] = mean (Offset)
			// voltageValues[3] = peak-peak
			// voltageValues[4] = rms

			ADCConvert(voltageValues, state);

			// Clear Screen
			LPUART_ESC_Code("[2J");
			// Return cursor home
			LPUART_ESC_Code("[H");
			// Print
			LPUART_print("AC MODE");
			// Move down 2 lines
			LPUART_ESC_Code("[2E");
			// Print to the terminal
			LPUART_print("Frequency: ");
			// Used to convert frequency to a string
			FreqToString(tempFreqString, frequency);
			LPUART_print(tempFreqString);
			// Print "Hz"
			LPUART_print(" Hz ");


			// Convert peak-peak value to string
			ADCVoltageToString(tempVoltageString, voltageValues[3]);
			// Print peak-to-peak values
			LPUART_print("| Peak-to-Peak: ");
			LPUART_print(tempVoltageString);
			LPUART_print(" V ");

			// Convert rms value to string
			ADCVoltageToString(tempVoltageString, voltageValues[4]);
			// Print rms values
			LPUART_print("| RMS Voltage: ");
			LPUART_print(tempVoltageString);
			LPUART_print(" V ");

			LPUART_ESC_Code("[1E");
			LPUART_print("Min: ");
			ADCVoltageToString(tempVoltageString, voltageValues[0]);
			LPUART_print(tempVoltageString);
			LPUART_print(" V ");

			LPUART_print("| Max: ");
			ADCVoltageToString(tempVoltageString, voltageValues[1]);
			LPUART_print(tempVoltageString);
			LPUART_print(" V ");

			LPUART_print("| Mean: ");
			ADCVoltageToString(tempVoltageString, voltageValues[2]);
			LPUART_print(tempVoltageString);
			LPUART_print(" V ");

			LPUART_ESC_Code("[2E");
			LPUART_print("RMS Voltage:");
			LPUART_ESC_Code("[E");

			serialPrintCount = (voltageValues[4] * 12) / 100;

			for (int i = 0; i <= serialPrintCount; i++)
				LPUART_print("#");

			LPUART_ESC_Code("[E");
			LPUART_print("|-----|-----|-----|-----|-----|-----|");
			LPUART_ESC_Code("[1E");
			LPUART_print("0    0.5   1.0   1.5   2.0   2.5   3.0");
			LPUART_ESC_Code("[1E");

			delay_us(100000);

			// Check if need to change state again (speeds up transition)
			if (changeStateFlag){
				state = ST_SEL;
				break;
			}
			break;
		}

		case ST_SEL:{
			// Clear change state flag
			changeStateFlag = 0;

			// Erase Whole Screen
			LPUART_ESC_Code("[2J");

			// Switch statement to check what was typed
			switch(stateChangeValue){

			// If A change to AC
			case 'A':{
				state = ST_AC;
				// Enable Comparator
				COMP1->CSR |= (COMP_CSR_EN);
				break;
				// If D change to DC
			} case 'D':{
				// Disable Comparator
				COMP1->CSR &= ~(COMP_CSR_EN);
				state = ST_DC;
				break;
				// If none of that do nothing
			} default:{
				break;
			}
			}
		}
		}
	}
}

/* -- Timer 2 Interrupt Handler -- */
void TIM2_IRQHandler(void){

	switch(stateVal){

	// If interrupted before variable set, do nothing
	case 0:{
		break;
	}

	// DC and AC Voltage Measurements
	case 1:{
		// Disable CCR1 interrupts
		TIM2->DIER &= ~(TIM_DIER_CC1IE);

		// Increment CCR1
		TIM2->CCR1 += CCR_Value;

		break;
	}

	// Frequency Measurements
	case 2:{
		// Stuff for the comparator
		if (compCount == 0){
			// Read CCR4 and save
			CCR_Capture = TIM2->CCR4;
			compCount++;
		} else if (compCount == 1){
			// Read CCR4 and save
			CCR_Capture2 = TIM2->CCR4;
			compCount++;
			// Disable interrupts
			TIM2->DIER &= ~(TIM_DIER_CC4IE);
		}
		break;
	}
	}
}

void LPUART1_IRQHandler(void){
	// Read the input
	stateChangeValue = LPUART1->RDR;

	// If interrupt occurs, state change may be needed
	changeStateFlag = 1;
}



/* -------------------------------------------- */
void SystemClock_Config(void)
{
	RCC_OscInitTypeDef RCC_OscInitStruct = {0};
	RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

	/** Configure the main internal regulator output voltage
	 */
	if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
	{
		Error_Handler();
	}

	/** Initializes the RCC Oscillators according to the specified parameters
	 * in the RCC_OscInitTypeDef structure.
	 */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI;
	RCC_OscInitStruct.MSIState = RCC_MSI_ON;
	RCC_OscInitStruct.MSICalibrationValue = 0;
	RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_9;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
	{
		Error_Handler();
	}

	/** Initializes the CPU, AHB and APB buses clocks
	 */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
			|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_MSI;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
	{
		Error_Handler();
	}
}
void Error_Handler(void)
{
	/* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */
	__disable_irq();
	while (1)
	{
	}
	/* USER CODE END Error_Handler_Debug */
}
#ifdef  USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
	/* USER CODE BEGIN 6 */
	/* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
	/* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
