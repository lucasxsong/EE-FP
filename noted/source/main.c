/*	Author: lsong013
 *	Project: Final Project
 *	Exercise Description: [optional - include for your own benefit]
 *
 *	I acknowledge all content contained herein, excluding template or example
 *	code, is my own original work.
 */


#define F_CPU 16000000UL
#include <inttypes.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <util/delay.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "SPI.h"
#include "font.h"
#include "display.h"

#define SAMPLES 128

//ISR timing
#define ADC_TIME 2000 	// 2000 cycles = 125us * 16MHz
#define SLEEP_TIME 1975 // set SLEEP to occur slightly before ISR

//------------Start of borrowed code from Bruce Land--------------//
#define int2fix(a)   (((int)(a))<<8) 
#define float2fix(a) ((int)((a)*256.0)) 
#define fix2float(a) ((float)(a)/256.0)  

// Fast fixed point multiply assembler macro
#define multfix(a,b)          	  \
({                                \
int prod, val1=a, val2=b ;        \
__asm__ __volatile__ (            \ 
"muls %B1, %B2	\n\t"              \
"mov %B0, r0 \n\t"	               \ 
"mul %A1, %A2\n\t"	               \ 
"mov %A0, r1   \n\t"              \ 
"mulsu %B1, %A2	\n\t"          \ 
"add %A0, r0  \n\t"               \ 
"adc %B0, r1 \n\t"                \ 
"mulsu %B2, %A1	\n\t"          \ 
"add %A0, r0 \n\t"           \ 
"adc %B0, r1  \n\t"          \ 
"clr r1  \n\t" 		         \ 
: "=&d" (prod)               \
: "a" (val1), "a" (val2)      \
);                            \
prod;                        \
})

#define N_WAVE          128    /* size of FFT */
#define LOG2_N_WAVE     7     /* log2(N_WAVE) */

//FFT buffer
#define spectrum_bins 32 			// amount of bins to send/display
int fftarray[N_WAVE];				// array to hold FFT points
char specbuff[spectrum_bins];		// array to hold freq bin data to transmit
char erasespecbuff[spectrum_bins];	// empty array to clear spec buff
unsigned char currbin;				// index of specbuff

// ADC Variables
volatile signed int adcbuff[N_WAVE];	// array to hold ADC audio sample points
volatile char adcind;					// index of adcbuff
int adcMask[N_WAVE];					// trapezoidal windowing function for ADC buffer


int Sinewave[N_WAVE]; // a table of sines for the FFT
signed int fr[N_WAVE],fi[N_WAVE],erasefi[N_WAVE];	// arrays used by FFT to store real, imaginary data, and a blank erase array

//===================================
//FFT function
void FFTfix(int fr[], int fi[], int m)
//Adapted from code by:
//Tom Roberts 11/8/89 and Malcolm Slaney 12/15/94 malcolm@interval.com
//fr[n],fi[n] are real,imaginary arrays, INPUT AND RESULT.
//size of data = 2**m 
// This routine does foward transform only
{
    int mr,nn,i,j,L,k,istep, n;
    int qr,qi,tr,ti,wr,wi; 
    
    mr = 0;
    n = 1<<m;   //number of points
    nn = n - 1;

    /* decimation in time - re-order data */
    for(m=1; m<=nn; ++m) 
    {
        L = n;
        do L >>= 1; while(mr+L > nn);
        mr = (mr & (L-1)) + L;
        if(mr <= m) continue;
        tr = fr[m];
        fr[m] = fr[mr];
        fr[mr] = tr;
        //ti = fi[m];   //for real inputs, don't need this
        //fi[m] = fi[mr];
        //fi[mr] = ti;
    }
    
    L = 1;
    k = LOG2_N_WAVE-1;
    while(L < n) 
    {
        istep = L << 1;
        for(m=0; m<L; ++m) 
        {
            j = m << k;
            wr =  Sinewave[j+N_WAVE/4];
            wi = -Sinewave[j];
            wr >>= 1;
            wi >>= 1;
            
            for(i=m; i<n; i+=istep) 
            {
                j = i + L;
                tr = multfix(wr,fr[j]) - multfix(wi,fi[j]);
                ti = multfix(wr,fi[j]) + multfix(wi,fr[j]);
                qr = fr[i] >> 1;
                qi = fi[i] >> 1;     
                fr[j] = qr - tr;
                fi[j] = qi - ti;
                fr[i] = qr + tr;
                fi[i] = qi + ti;
            }
        }
        --k;
        L = istep;
    }
}
//------------End of borrowed code from Bruce Land--------------//



void ADC_init() {
	//ADCSRA |= (1 << ADEN) | (1 << ADSC) | (1 << ADATE);
	ADMUX = (1<<ADLAR)|(1<<REFS1)|(1<<REFS0)+0;				// Enable ADC Left Adjust Result and 2.56V Voltage Reference and ADC Port 0
	ADCSRA = ((1<<ADEN)|(1<<ADSC))+7; 
	adcind = 0;
	currbin = 0;
}

ISR(TIMER1_COMPB_vect, ISR_NAKED)
{
	sei();
	sleep_cpu();
	reti();
}


ISR (TIMER1_COMPA_vect) {
	if (adcind < 0) { adcind = 0;}
	if(adcind<N_WAVE) {	// if ADC buffer isn't full...
		//store an ADC sample and start the next one
		adcbuff[adcind++]=ADCH-140;		// subtract 140 to remove DC offset, corresponds to about 1.4V
		ADCSRA |= (1<<ADSC);
	}
}

unsigned char x;
char buf[128];
unsigned l;
unsigned m;
unsigned h;
char allMode[40] = "all bin display \n";
char low[40] = "low frequency";
char medium[40] = "med frequency";
char high[40] = "high frequency";
char no[40] = "no sound";
int index = 0;
int j;

// guess mode tries to guess what frequency
// all mode displays all bins
enum states {start, dGuess, dAll, waitUpG, waitUpA} states;

void tick() {
	unsigned char button = ~PINC & 0x01;
	switch(states) {
		case start:
			states = dAll;
			break;
		case dAll:
			if (button) { states = waitUpG;}
			//states = dAll;
			break;
		case waitUpG:
			if (!button) { states = dGuess;}
			//states = waitUpG;
			break;
		case dGuess:
			if (button) {states = waitUpA;}
			//states = dGuess;
			break;
		case waitUpA:
			if (!button) {states = dAll;}
			//states = waitUpA;
			break;
		default:
			states = start;
			break;
	}
	switch(states) {
		case start:
			break;
		case dAll:
			PORTD = 0x01;
			index = 0;
			lcd_setXY(0x40,0x80);
		  	memset(buf, 0, 128);
			strcpy(buf, allMode);
			for (j=3; j<32; j++) {
	   			index += sprintf(&buf[index], "%d ", specbuff[j]);
			}
			break;
		case waitUpG:
			memset(buf, 0, 128);
			break;
		case dGuess:
			PORTD = 0x02;
			index = 0;
			lcd_setXY(0x40,0x80);
		  	memset(buf, 0, 128);
			//for (j=2; j<32; j++) {
	   		//	index += sprintf(&buf[index], "%d ", specbuff[j]);
			//}
			l = 0;
			m = 0;
			h = 0;
			for (j = 3; j < 13; j++) {
				l+= specbuff[j];
			}
			for (j = 13; j < 23; j++) {
				m+= specbuff[j];
			}
			for (j = 23; j < 32; j++) {
				h+= specbuff[j];
			}
			
			if (l < 1 && m < 1 && h < 1) { strcpy(buf, no);}
			else if (l > m && l > h) { strcpy(buf, low);}
			else if (m > l && m > h) { strcpy(buf, medium);}
			else if (h > l && h > m) { strcpy(buf, high);}
			break;
		case waitUpA:
			memset(buf, 0, 128);
			break;
		default:
			break;
	}
}
 

int main()
{
	DDRC = 0x00;
	PORTC = 0xFF;
	DDRD = 0xFF;
	PORTD = 0x00;
	SPI_Init();
	N5110_init();
	N5110_clear();
	lcd_setXY(0x40,0x80);
	DDRD = 0xFF; PORTD = 0x00;

	//init timer 1 to sample audio
  	// TIMER 1: OC1* disconnected, CTC mode, fosc/1 (16MHz), OC1A and OC1B interrupts enabled
  	TCCR1B = _BV(WGM12) | _BV(CS10);
  	OCR1A = ADC_TIME;	// time for one ADC sample
  	OCR1B = SLEEP_TIME;	// time to go to sleep
  	TIMSK1 = _BV(OCIE1B) | _BV(OCIE1A);

	ADC_init();
	adcind = 0;
	currbin = 0;

	//loop iterator
	int i;
	// generate arrays
	for (i=0; i<N_WAVE; i++) {
	// Set up FFT, one cycle sine table required for FFT
	Sinewave[i] = float2fix(sin(6.283*((float)i)/N_WAVE)); 
	// generate empty array to erase
	erasefi[i]=0;
	// generate trapezoid mask (with 1/4 length slopes) for ADC buffer
	if(i<32) adcMask[i] = float2fix((8*(float)i/255));
	else if(i >= 32 && i <= 96) adcMask[i] = 0x0100;
	else if(i > 96) adcMask[i] = float2fix(((128-(float)i)*8/255));
	}
	// generate empty array to erase
	for (i=0; i<spectrum_bins; i++)
	erasespecbuff[i]=0;

	// Set up single ADC timing with sleep mode
	sei();
	set_sleep_mode(SLEEP_MODE_IDLE);
	sleep_enable();

	int freqopt = 1;

	int index = 0;
	
	while (1) {
		// clear fft arrays
		memcpy(specbuff,erasespecbuff,spectrum_bins);
		memcpy(fi,erasefi,N_WAVE);

		// copy adc buffer into separate array
		memcpy(fr, adcbuff, N_WAVE);
		// scale  
		for(i=0; i<N_WAVE; i++){
			fr[i] = multfix((fr[i]<<4),adcMask[i]);
		}
	
		FFTfix(fr, fi, LOG2_N_WAVE);

		for (i=0;i<(N_WAVE/2);i++) {
			//Magnitude Function: Sum of Squares of the Real & Imaginary parts
			fftarray[i]=multfix(fr[i],fr[i])+multfix(fi[i],fi[i]);
			//store values into 32 frequency bins depending on overall frequency 
			if (freqopt==0) specbuff[(char)(i/2)]+=(char)(fftarray[i]);
			else if (freqopt==1 && i<32) specbuff[i]+=(char)(fftarray[i]);
		} 
		adcind = 0;
		currbin = 0;
		
		_delay_ms(100);
		N5110_clear();

		tick();
		
		N5110_Data(buf);				

		
    	}
	return 0;
}
