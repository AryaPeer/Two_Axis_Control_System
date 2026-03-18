/**
 ******************************************************************************
 * @file    stm32f4xx_it.c
 * @date    13/05/2015 09:14:38
 * @brief   Interrupt Service Routines.
 ******************************************************************************
 *
 * COPYRIGHT(c) 2015 STMicroelectronics
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
/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_it.h"
#include "xnucleoihm02a1_interface.h"
#include "example_usart.h"
#include "L6470.h"

volatile uint8_t motor_select = 0;    /* 0=L/R axis, 1=up/down axis */

/* 0=no limit, 1=forward limit hit, 2=reverse limit hit */
volatile uint8_t last_limit_hit_0 = 0;
volatile uint8_t last_limit_hit_1 = 0;

/**
 * @addtogroup MicrosteppingMotor_Example
 * @{
 */

/**
 * @addtogroup STM32F4XX_IT
 * @{
 */

/******************************************************************************/
/*            Cortex-M4 Processor Interruption and Exception Handlers         */
/******************************************************************************/

/**
 * @addtogroup STM32F4XX_IT_Exported_Functions
 * @{
 */

/**
 * @brief This function handles System tick timer.
 */
void SysTick_Handler(void)
{
  HAL_IncTick();
  HAL_SYSTICK_IRQHandler();
}

/******************************************************************************/
/* STM32F4xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32f4xx.s).                    */
/******************************************************************************/

/**
 * @brief This function handles EXTI Line1 interrupt.
 */
void EXTI1_IRQHandler(void)
{
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_1);
}

/**
 * @brief This function handles EXTI Line0 interrupt.
 */
void EXTI0_IRQHandler(void)
{
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_0);
}

/**
 * @brief This function handles USART2 global interrupt.
 */
void USART2_IRQHandler(void)
{
  HAL_UART_IRQHandler(&huart2);
  USART_ITCharManager(&huart2);
}

/**
 * @brief This function handles EXTI Line[15:10] interrupts.
 */
void EXTI15_10_IRQHandler(void)
{
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_13);
}

/* PB4 motor-select button */
void EXTI4_IRQHandler(void)
{
  if (__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_4) != RESET)
  {
    for (volatile int i = 0; i < 100000; i++) {}

    __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_4);

    if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_4) != GPIO_PIN_RESET)
    {
      L6470_HardStop(motor_select);
      motor_select ^= 1;
    }
  }
}

static inline void handle_limit_switch(
    uint16_t pin, GPIO_TypeDef *port,
    GPIO_TypeDef *other_port, uint16_t other_pin,
    uint8_t motor_id, uint8_t limit_id,
    volatile uint8_t *limit_flag)
{
  if (__HAL_GPIO_EXTI_GET_IT(pin) != RESET)
  {
    __HAL_GPIO_EXTI_CLEAR_IT(pin);
    for (volatile int i = 0; i < 10000; i++) {}

    /* Reject spurious trigger if the paired switch on the same axis is also asserted */
    if (HAL_GPIO_ReadPin(port, pin) != GPIO_PIN_RESET &&
        HAL_GPIO_ReadPin(other_port, other_pin) == GPIO_PIN_RESET)
    {
      L6470_HardStop(motor_id);
      *limit_flag = limit_id;
    }
  }
}

void EXTI9_5_IRQHandler(void)
{
  /* motor 0 (L/R): PB6 forward, PB8 reverse */
  handle_limit_switch(GPIO_PIN_6, GPIOB, GPIOB, GPIO_PIN_8, 0, 1, &last_limit_hit_0);
  handle_limit_switch(GPIO_PIN_8, GPIOB, GPIOB, GPIO_PIN_6, 0, 2, &last_limit_hit_0);

  /* motor 1 (up/down): PC7 forward, PB9 reverse */
  handle_limit_switch(GPIO_PIN_7, GPIOC, GPIOB, GPIO_PIN_9, 1, 1, &last_limit_hit_1);
  handle_limit_switch(GPIO_PIN_9, GPIOB, GPIOC, GPIO_PIN_7, 1, 2, &last_limit_hit_1);
}

/**
 * @}
 */
/* End of STM32F4XX_IT_Exported_Functions */

/**
 * @}
 */
/* End of STM32F4XX_IT */

/**
 * @}
 */
/* End of MicrosteppingMotor_Example */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
