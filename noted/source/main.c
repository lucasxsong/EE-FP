/*	Author: lsong013
 *	Project: Final Project
 *	Exercise Description: [optional - include for your own benefit]
 *
 *	I acknowledge all content contained herein, excluding template or example
 *	code, is my own original work.
 */


#define F_CPU 8000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <string.h>
#include "SPI.h"
#include "font.h"
#include "display.h"
#include "ffft.h"

#define SAMPLES 128

void ADC_init() {
	ADCSRA |= (1 << ADEN) | (1 << ADSC) | (1 << ADATE);
}

int16_t capture[SAMPLES];
complex_t bfly_buff[SAMPLES];
uint16_t spectrum[SAMPLES/2];

void setup() {
	//set up bins

	// set up sampling period
	//sampling_period_us = round(1000000 * (1.0/SAMPLING_FREQUENCY));
}

void sample() {
	for (int i = 0; i < SAMPLES; i++) {
		// read from ADC
		capture[i] = ADC;
	}
}

int main()
{
	SPI_Init();
	N5110_init();
	N5110_clear();
	lcd_setXY(0x40,0x80);
	DDRD = 0xFF; PORTD = 0x00;

	long int x;
	ADC_init();
	while (1) { 
		sample(); // fill capture buffer with data

		fft_input(capture, bfly_buff); // compute samples -> complex numbers
		//fft_execute(bfly_buff); // compute complex data
		//fft_output(bfly_buff, spectrum);		

		_delay_ms(300); 
		N5110_clear();
		lcd_setXY(0x40,0x80);

		x = spectrum[2];

	  	char buf[40];
      		snprintf(buf, 40, "adc is %d :)", x); // puts string into buffer
		
		N5110_Data(buf);	
    	}
	return 0;
}
