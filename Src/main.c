/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2019 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "dma.h"
#include "usart.h"
#include "gpio.h"
#include <stdio.h>
#include <string.h>

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);


/* Function processing DMA Rx data. Counts how many capital and small letters are in sentence.
 * Result is supposed to be stored in global variable of type "letter_count_" that is defined in "main.h"
 *
 * @param1 - received sign
 */
void proccesDmaData(uint8_t sign);


/* Space for your global variables. */

#define TEXT_LENGTH 35
#define RESULT_COUNT 16

static letter_count_ letterCount;
static struct
{
	char text[TEXT_LENGTH + 1];
	letter_count_ letters;
} results[RESULT_COUNT];
static volatile uint8_t readyResults;
volatile uint32_t milliseconds;


int main(void)
{
  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SYSCFG);
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);
  NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);

  /* Configure the system clock */
  SystemClock_Config();
  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART2_UART_Init();

  /* Space for your local variables, callback registration ...*/

  USART2_RegisterCallback(proccesDmaData);
  static uint8_t txMessage[128];
  uint8_t nextResult = 0;
  uint32_t lastStatus = 0;
  LL_SYSTICK_EnableIT();

  while (1)
  {
	  /* Periodic transmission of information about DMA Rx buffer state.
	   * Transmission frequency - 0.5Hz.
	   * Message format - "Buffer capacity: %d bytes, occupied memory: %d bytes, load [in %]: %f%"
	   * Example message (what I wish to see in terminal) - Buffer capacity: 1000 bytes, occupied memory: 231 bytes, load [in %]: 23.1%
	   */

	  /* Valid text string information transmission.
	   * Transmission frequency - when new valid string is received.
	   * Message format - "Valid string: %s, lower-case: %d, upper-case: %d"
	   * Example message (what I wish to see in terminal) - Valid string: Platn15uborZnakov, lower-case: 13, upper-case: 2
	   */

	  if(nextResult == readyResults)
	  {
		  uint32_t mask = __get_PRIMASK();
		  __disable_irq();
		  if(nextResult == readyResults)
		  {
			  nextResult = 0;
			  readyResults = 0;
		  }
		  __set_PRIMASK(mask);
	  }

	  if(!LL_DMA_IsEnabledChannel(DMA1, LL_DMA_CHANNEL_7))
	  {
		  int length = 0;
		  uint32_t now = milliseconds;
		  if((uint32_t)(now - lastStatus) >= 2000)
		  {
			  uint32_t occupied = DMA_USART2_BUFFER_SIZE - LL_DMA_GetDataLength(DMA1, LL_DMA_CHANNEL_6);
			  uint32_t load = (occupied * 1000 + DMA_USART2_BUFFER_SIZE / 2) / DMA_USART2_BUFFER_SIZE;
			  length = snprintf((char *)txMessage, sizeof(txMessage),
					  "Buffer capacity: %u bytes, occupied memory: %u bytes, load [in %%]: %u.%u%%\r\n",
					  (unsigned int)DMA_USART2_BUFFER_SIZE, (unsigned int)occupied,
					  (unsigned int)(load / 10), (unsigned int)(load % 10));
			  lastStatus = now;
		  }
		  else if(nextResult < readyResults)
		  {
			  __DMB();
			  length = snprintf((char *)txMessage, sizeof(txMessage),
					  "Valid string: %s, lower-case: %u, upper-case: %u\r\n",
					  results[nextResult].text, (unsigned int)results[nextResult].letters.small_letter,
					  (unsigned int)results[nextResult].letters.capital_letter);
			  nextResult++;
		  }
		  if(length > 0 && (size_t)length < sizeof(txMessage))
		  {
			  USART2_PutBuffer(txMessage, (uint8_t)length);
		  }
	  }
  }
  /* USER CODE END 3 */
}


void SystemClock_Config(void)
{
  LL_FLASH_SetLatency(LL_FLASH_LATENCY_0);

  if(LL_FLASH_GetLatency() != LL_FLASH_LATENCY_0)
  {
  Error_Handler();  
  }
  LL_RCC_HSI_Enable();

   /* Wait till HSI is ready */
  while(LL_RCC_HSI_IsReady() != 1)
  {
    
  }
  LL_RCC_HSI_SetCalibTrimming(16);
  LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
  LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_1);
  LL_RCC_SetAPB2Prescaler(LL_RCC_APB1_DIV_1);
  LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_HSI);

   /* Wait till System clock is ready */
  while(LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_HSI)
  {
  
  }
  LL_Init1msTick(8000000);
  LL_SYSTICK_SetClkSource(LL_SYSTICK_CLKSOURCE_HCLK);
  LL_SetSystemCoreClock(8000000);
}

/*
 * Implementation of function processing data received via USART.
 */
void proccesDmaData(uint8_t sign)
{
	/* Process received data */

	static char text[TEXT_LENGTH + 1];
	static uint8_t length = 0;
	static uint8_t started = 0;

	if(!started)
	{
		if(sign == '#')
		{
			started = 1;
			length = 0;
			letterCount.small_letter = 0;
			letterCount.capital_letter = 0;
		}
		return;
	}

	if(sign == '$')
	{
		text[length] = '\0';
		uint8_t slot = readyResults;
		if(slot < RESULT_COUNT)
		{
			memcpy(results[slot].text, text, length + 1);
			results[slot].letters = letterCount;
			__DMB();
			readyResults = slot + 1;
		}
		started = 0;
		return;
	}

	if(length == TEXT_LENGTH)
	{
		started = 0;
		letterCount.small_letter = 0;
		letterCount.capital_letter = 0;
		return;
	}

	text[length++] = sign;
	if(sign >= 'a' && sign <= 'z')
	{
		letterCount.small_letter++;
	}
	else if(sign >= 'A' && sign <= 'Z')
	{
		letterCount.capital_letter++;
	}
}


void Error_Handler(void)
{

}

#ifdef  USE_FULL_ASSERT

void assert_failed(char *file, uint32_t line)
{ 

}

#endif

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
