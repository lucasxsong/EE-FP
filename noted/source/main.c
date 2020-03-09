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
#include <stdio.h>
#include <math.h>
//#include <complex.h>
#include "SPI.h"
#include "font.h"
#include "display.h"

#include "fft.h"
#include "ift.h"

#define SAMPLES 128

#define cplx float _Complex

void ADC_init() {
	ADCSRA |= (1 << ADEN) | (1 << ADSC) | (1 << ADATE);
}
 

int main()
{
	SPI_Init();
	N5110_init();
	N5110_clear();
	lcd_setXY(0x40,0x80);
	DDRD = 0xFF; PORTD = 0x00;

	int x;
	ADC_init();
	
	// sets up kiss-fft real signal to complex frequency
	cplx buf[128];
	
	while (1) { 
		for (int i = 0; i < SAMPLES; i++) {
			// read from ADC
			buf[i] = ADC;
		}

		fft(buf, 128);
	
		_delay_ms(300); 
		N5110_clear();
		lcd_setXY(0x40,0x80);

		x = buf[2];

	  	char buf[40];
      		snprintf(buf, 40, "adc is %d :)", x); // puts string into buffer
		
		N5110_Data(buf);	
    	}
	return 0;
}
