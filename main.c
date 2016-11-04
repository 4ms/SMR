/*
 * main.c - Spectral Mulitband Resonator v1.0
 *
 * Author: Dan Green (danngreen1@gmail.com)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * See http://creativecommons.org/licenses/MIT/ for more information.
 *
 * -----------------------------------------------------------------------------
 */


#include <stm32f4xx.h>

#include "codec.h"
#include "i2s.h"
#include "adc.h"
#include "audio_util.h"
#include "dig_inouts.h"
#include "globals.h"
#include "filter.h"
#include "envout_pwm.h"
#include "led_ring.h"
#include "dac.h"
#include "rotary.h"
#include "rotation.h"
#include "params.h"
#include "system_mode.h"
#include "user_scales.h"
#include "limiter.h"
//#include "compressor.h"


__IO uint16_t adc_buffer[NUM_ADCS];
__IO uint16_t potadc_buffer[NUM_ADC3S];

extern enum UI_Modes ui_mode;

extern uint8_t flag_update_LED_ring;


extern uint8_t do_ROTUP;
extern uint8_t do_ROTDOWN;

extern int8_t spread;

extern uint8_t cur_param_bank;


#define delay()						\
do {							\
  register unsigned int i;				\
  for (i = 0; i < 1000000; ++i)				\
    __asm__ __volatile__ ("nop\n\t":::"memory");	\
} while (0)

void check_errors(void){

}

inline uint32_t diff(uint32_t a, uint32_t b){
	return (a>b)?(a-b):(b-a);
}


void main(void)
{
	int32_t t_spread;
	uint32_t i;

    NVIC_SetVectorTable(NVIC_VectTab_FLASH, 0x8000);
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);	//Set Priority Grouping mode to 2-bits for priority and 2-bits for sub-priority

	LED_ON(LED_RING_OE); //actually turns the LED ring off
	LEDDriver_Init(5);
	for (i=0;i<26;i++)	LEDDriver_setRGBLED(i,0);
	LED_OFF(LED_RING_OE); //actually turns the LED ring on

	flag_update_LED_ring=1;


	init_inouts();
	init_rotary();
	init_envout_pwm();

	ADC1_Init((uint16_t *)adc_buffer);
	ADC3_Init((uint16_t *)potadc_buffer);

	Codec_Init(SAMPLERATE);

	delay();

	set_default_param_values();

	set_default_color_scheme();

	set_default_user_scalebank();

	//overwrite default parameters if a startup bank exists
	cur_param_bank = load_startup_params();

	if (cur_param_bank == 0xFF){
		factory_reset();
		cur_param_bank=0;
	}

	I2S_Block_Init();

	TIM6_Config();
	DAC_Ch1_NoiseConfig();

	spread=(adc_buffer[SPREAD_ADC] >> 8) + 1;

	I2S_Block_PlayRec();

	//update_spread(1);

	init_freq_update_timer();
	init_ENV_update_timer();

	while(1){

		check_errors();

		param_read_switches();

		update_LED_ring();

		update_lock_leds();

		t_spread=read_spread();

		if (t_spread!=-1) update_spread(t_spread);

		process_lock_jacks();

		process_lock_buttons();

		process_rotary_button();

		process_rotary_rotation();

		if (ui_mode==PLAY)
			check_rotary_pressed_repeatedly();

		if (ui_mode==EDIT_SCALES){
			handle_edit_scale();
			handle_edit_tracking();
			handle_edit_voct();
		}

		if (ui_mode==EDIT_COLORS)
			handle_edit_colors();

		if (ui_mode==SELECT_PARAMS){
			handle_freqpot_changing_filtermode();
			handle_freqpot_changing_filtermode_mode();
			handle_slider_changing_clipmode();
		}

		if (do_ROTUP){
			do_ROTUP=0;
			rotate_up();
		}


		if (do_ROTDOWN){
			do_ROTDOWN=0;
			rotate_down();
		}

		process_rotateCV();

		process_scaleCV();

	} //end main loop


} //end main()

#ifdef  USE_FULL_ASSERT

#define assert_param(expr) ((expr) ? (void)0 : assert_failed((uint8_t *)__FILE__, __LINE__))

/**
  * @brief  Reports the name of the source file and the source line number
  *   where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t* file, uint32_t line)
{ 
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

  /* Infinite loop */
  while (1)
  {
  }
}
#endif

#if 1
/* exception handlers - so we know what's failing */
void NMI_Handler(void)
{ 
	while(1){};
}

void hard_fault_handler_c ( unsigned int * hardfault_args ) __attribute__( ( naked ) );
void hard_fault_handler_c (unsigned int * hardfault_args)
{
  unsigned int stacked_r0;
  unsigned int stacked_r1;
  unsigned int stacked_r2;
  unsigned int stacked_r3;
  unsigned int stacked_r12;
  unsigned int stacked_lr;
  unsigned int stacked_pc;
  unsigned int stacked_psr;

  stacked_r0 = ((unsigned long) hardfault_args[0]);
  stacked_r1 = ((unsigned long) hardfault_args[1]);
  stacked_r2 = ((unsigned long) hardfault_args[2]);
  stacked_r3 = ((unsigned long) hardfault_args[3]);

  stacked_r12 = ((unsigned long) hardfault_args[4]);
  stacked_lr = ((unsigned long) hardfault_args[5]);
  stacked_pc = ((unsigned long) hardfault_args[6]);
  stacked_psr = ((unsigned long) hardfault_args[7]);

  while (1){
	  LED_SLIDER_GPIO->BSRRH = (uint16_t)hardfault_args[5];
	  LED_SLIDER_GPIO->BSRRL = (uint16_t)hardfault_args[6];
  }

}


/*
void HardFault_Handler(void)
{
	while(1){};
}
*/

void MemManage_Handler(void)
{ 
	while(1){};
}

void BusFault_Handler(void)
{ 
	while(1){};
}

void UsageFault_Handler(void)
{ 
	while(1){};
}

void SVC_Handler(void)
{ 
	while(1){};
}

void DebugMon_Handler(void)
{ 
	while(1){};
}

void PendSV_Handler(void)
{ 
	while(1){};
}
#endif
