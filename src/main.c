/**
 ******************************************************************************
 * File Name          : main.c
 * Date               : 09/10/2014 11:13:03
 * Description        : Main program body
 ******************************************************************************
 *
 * COPYRIGHT(c) 2014 STMicroelectronics
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *   1. Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright notice,
 *      this list of conditions and the following disclaimer in the documentation
 *      and/or other materials provided with the distribution.
 *   3. Neither the name of STMicroelectronics nor the names of its contributors
 *      may be used to endorse or promote products derived from this software
 *      without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ******************************************************************************
 */

#include "example.h"
#include "example_usart.h"
#include "L6470.h"
#include "stm32f4xx_it.h"

/**
 * @defgroup   MotionControl
 * @{
 */

/**
 * @addtogroup BSP
 * @{
 */

/**
 * @}
 */
/* End of BSP */

/**
 * @addtogroup MicrosteppingMotor_Example
 * @{
 */

/**
 * @defgroup   ExampleTypes
 * @{
 */

// #define MICROSTEPPING_MOTOR_EXAMPLE        //!< Uncomment to performe the standalone example
#define MICROSTEPPING_MOTOR_USART_EXAMPLE //!< Uncomment to performe the USART example
#if ((defined(MICROSTEPPING_MOTOR_EXAMPLE)) && (defined(MICROSTEPPING_MOTOR_USART_EXAMPLE)))
#error "Please select an option only!"
#elif ((!defined(MICROSTEPPING_MOTOR_EXAMPLE)) && (!defined(MICROSTEPPING_MOTOR_USART_EXAMPLE)))
#error "Please select an option!"
#endif
#if (defined(MICROSTEPPING_MOTOR_USART_EXAMPLE) && (!defined(NUCLEO_USE_USART)))
#error "Please define "NUCLEO_USE_USART" in "stm32fxxx_x-nucleo-ihm02a1.h"!"
#endif

/**
 * @}
 */
/* End of ExampleTypes */

/**
 * @brief The FW main module
 */

extern volatile uint8_t motor_select;
/* 0=clear, 1=forward limit hit, 2=reverse limit hit */
extern volatile uint8_t last_limit_hit_0;
extern volatile uint8_t last_limit_hit_1;

#define MOTOR0_SPEED  3000   /* steps/s, L/R axis */
#define MOTOR1_SPEED  18000  /* steps/s, up/down axis */

static inline uint8_t direction_blocked(uint8_t limit_flag, uint8_t direction)
{
  return (limit_flag == 1 && direction == 1) ||
         (limit_flag == 2 && direction == 0);
}

int main(void)
{
  /* NUCLEO board initialization */
  NUCLEO_Board_Init();
  MX_ADC1_Init();
  /* X-NUCLEO-IHM02A1 initialization */
  BSP_Init();

#ifdef NUCLEO_USE_USART
  /* Transmit the initial message to the PC via UART */
  USART_TxWelcomeMessage();
  USART_Transmit(&huart2, " X-CUBE-SPN2 v1.0.0\n\r");
#endif

#if defined(MICROSTEPPING_MOTOR_EXAMPLE)
  /* Perform a batch commands for X-NUCLEO-IHM02A1 */
  MicrosteppingMotor_Example_01();

  /* Infinite loop */
  while (1)
    ;
#elif defined(MICROSTEPPING_MOTOR_USART_EXAMPLE)
  /* Fill the L6470_DaisyChainMnemonic structure */
  Fill_L6470_DaisyChainMnemonic();

  /*Initialize the motor parameters */
  Motor_Param_Reg_Init();

  GPIO_InitTypeDef GPIO_InitStruct;

  /* Limit switches: 4 total, 2 per axis, rising-edge IRQ */
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FAST;

  GPIO_InitStruct.Pin = GPIO_PIN_6;   /* motor 0 forward */
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_8;   /* motor 0 reverse */
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_7;   /* motor 1 forward */
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_9;   /* motor 1 reverse */
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* Motor-select button */
  GPIO_InitStruct.Pin = GPIO_PIN_4;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* Pot input (ADC1_IN8) */
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  HAL_NVIC_SetPriority(EXTI9_5_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
  HAL_NVIC_SetPriority(EXTI4_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI4_IRQn);

  while (1)
  {
    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY);
    uint32_t adc_value = HAL_ADC_GetValue(&hadc1);

    /* Pot at either extreme = rest */
    if (adc_value > 3900 || adc_value < 100)
    {
      L6470_HardStop(motor_select);
      continue;
    }

    uint8_t direction = (adc_value > 2048) ? 1 : 0;

    if (motor_select == 0)
    {
      if (direction_blocked(last_limit_hit_0, direction))
        continue;

      /* Re-check inside critical section: ISR may have set the flag
       * between the check above and disable_irq. */
      __disable_irq();
      if (!direction_blocked(last_limit_hit_0, direction))
      {
        last_limit_hit_0 = 0;
        L6470_HardStop(1);
        L6470_Run(0, direction, MOTOR0_SPEED);
      }
      __enable_irq();
    }
    else
    {
      if (direction_blocked(last_limit_hit_1, direction))
        continue;

      __disable_irq();
      if (!direction_blocked(last_limit_hit_1, direction))
      {
        last_limit_hit_1 = 0;
        L6470_HardStop(0);
        L6470_Run(1, direction, MOTOR1_SPEED);
      }
      __enable_irq();
    }
  }
#endif
}

#ifdef USE_FULL_ASSERT

/**
 * @brief Reports the name of the source file and the source line number
 * where the assert_param error has occurred.
 * @param file: pointer to the source file name
 * @param line: assert_param error line source number
 * @retval None
 */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
    ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}

#endif

/**
 * @}
 */
/* End of MicrosteppingMotor_Example */

/**
 * @}
 */
/* End of MotionControl */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
