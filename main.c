/*
 * PWM is generated by TCB0 on 8-Bit PWM Mode.
 * The period of the waveform is set to 256 clock cycles.
 * Pulse width is set by writing to the TCB0.CCMPH register.
 * The PWM is output on PORTA pin 5.
 *
 * The sampling period is a multiple of the PWM period, decided
 * by SAMPLE_RATE_DIV, in order to avoid aliasing.
 * Interrupts for sampling the input and for setting the Pulse
 * width are generated by TCA0, which is set to have the sample
 * frequency.
 *
 * At the beginning of the sampling period the sampling interrupt
 * is generated.  Here the program samples the voltage at PORTB
 * pin 1 using ADC0, and then does all the necessary computations
 * for that sampling period.
 * Near the end of the sampling period (  ) an interrupt is
 * generated to update the Pulse Width.
 *
 * The PWM Clock and the Sample Clock are synchronized.
 *
 * The PWM frequency is		F_PWM		=	20'000'000 / 256 = 78'125
 * The Sample rate is 		F_SAMPLE	=	78'125 / SAMPLE_RATE_DIV
 */

#define F_CPU 20000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdio.h>
#define PWM_TOP 0xFF
#define PW_INT_OFFSET 40
#define SAMPLE_RATE_DIV 2

void set_clk(void);		// set clock speed to 20MHz
void adc_init(void);	// initialize ADC0 to read from PB1
void pwm_init(void);	// initialize TCB0 as pwm output on PA5
void smpclk_init(void);	// initialize TCA0 as sample clock

volatile uint8_t pw;
volatile int8_t buffer[0x80];
uint8_t idx = 0;
#define BUF(i) buffer[(i)&0x7F]

#define ENABLE_CHORUS 1
#define CHORUS_PERIOD 200
int8_t chorus(volatile int8_t *buffer, size_t size);
uint8_t chorus_delay = 0;
int8_t chorus_delay_inc = 1;
uint16_t i;

int8_t adc_curr;
int8_t adc_prev;

void set_clk(void) {
	CCP = CCP_IOREG_gc;
	CLKCTRL.MCLKCTRLB = 0x00; // disable clock prescaler
}

void adc_init(void) {
	VREF.CTRLA 		= 0x40;	// set ADC0 reference to 1.5V
	ADC0.CTRLC 		= ADC_PRESC_DIV8_gc
					| ADC_REFSEL_INTREF_gc;
	ADC0.CTRLA 		= ADC_ENABLE_bm
					| ADC_RESSEL_8BIT_gc;
	ADC0.MUXPOS 	= ADC_MUXPOS_AIN10_gc;
}

void pwm_init(void) {
	PORTA.DIRSET 	= PIN5_bm;
	TCB0.CTRLA		= TCB_CLKSEL_CLKDIV1_gc
					| TCB_SYNCUPD_bm
					| TCB_ENABLE_bm;
	TCB0.CTRLB		= TCB_CNTMODE_PWM8_gc
					| TCB_CCMPEN_bm;
	TCB0.CCMPL 		= PWM_TOP;
}

void smpclk_init(void) {
	TCA0.SINGLE.CTRLA 	= TCA_SINGLE_CLKSEL_DIV1_gc
						| TCA_SINGLE_ENABLE_bm;
	TCA0.SINGLE.CTRLB 	= TCA_SINGLE_WGMODE_SINGLESLOPE_gc;
	TCA0.SINGLE.PER 	= SAMPLE_RATE_DIV*(PWM_TOP+1)-1;
	TCA0.SINGLE.CMP0	= PWM_TOP - PW_INT_OFFSET;
	TCA0.SINGLE.INTCTRL = TCA_SINGLE_OVF_bm
						| TCA_SINGLE_CMP0_bm;
}

int8_t chorus(volatile int8_t *buffer, size_t size) {
	int result;
	result			= *(buffer + (idx+chorus_delay)%size)
					+ *(buffer + (idx-chorus_delay+1+size)%size)
					+ *(buffer + (idx+chorus_delay+0x40)%size)
					+ *(buffer + (idx-chorus_delay+0x41+size)%size);
	/*
	 * power of 2 size version
	 * result		= *(buffer + ((idx+chorus_delay)&(size-1)))
	 * 				+ *(buffer + ((idx-chorus_delay+1)&(size-1)))
	 * 				+ *(buffer + ((idx+chorus_delay+0x40)&(size-1)))
	 * 				+ *(buffer + ((idx-chorus_delay+0x41)&(size-1)));
	 */
	result = result <  0x7F ? result :  0x7F;
	result = result > -0x7F ? result : -0x7F;
	if (!i++) {
		chorus_delay++;
	}
	i %= CHORUS_PERIOD;
	return result;
}

ISR(TCA0_OVF_vect) { // Sampling Interrupt
	ADC0.COMMAND	= ADC_STCONV_bm;
	/* DC blocking HPF */
	adc_prev = adc_curr;
	adc_curr = ADC0.RESL - 0x7F; 
	BUF(idx) = BUF(idx-1) + adc_curr - adc_prev;
	BUF(idx) -= BUF(idx)>>4;
	/* Effect processing */
	if (ENABLE_CHORUS) {
		pw = chorus(buffer, sizeof(buffer)) + 0x7F;
	} else {
		pw = BUF(idx) + 0x7F;
	}

	idx++;
	TCA0.SINGLE.INTFLAGS	= TCA_SINGLE_OVF_bm;
}

ISR(TCA0_CMP0_vect) { // PW update Interrupt
	TCB0.CCMPH				= pw;
	TCA0.SINGLE.INTFLAGS	= TCA_SINGLE_CMP0_bm;
}

int main(void) {
	set_clk();
	adc_init();
	smpclk_init();
	pwm_init();
	sei();
	while (1);
}
