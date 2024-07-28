// ADC.c:  Shows how to use the 14-bit ADC.  This program
// measures the voltage from some pins of the EFM8LB1 using the ADC.
//
// (c) 2008-2018, Jesus Calvino-Fraga
//

#include <stdio.h>
#include <stdlib.h>
#include <EFM8LB1.h>
#include "lcd.h"
#include <string.h>

#define SYSCLK 72000000L
#define BAUDRATE 115200L
#define SARCLK 18000000L
#define pb_setting P0_6

int v1 = 0;
int buzz = 500;
int bflag = 0;

void Timer2_ISR(void) interrupt INTERRUPT_TIMER2
{
	// SFRPAGE=0x0;
	TF2H = 0; // Clear Timer2 interrupt flag

	v1++;
	if (v1 >= 1000)
	{
		v1 = 0;
	}

	if (v1 < buzz)
	{
		P3_0 = !P3_0;
	}
}

idata char buffer[20];

#define TIMER1_PRESCALER 12
void Timer1_Init()
{
	// Configure Timer 1 in 16-bit auto-reload mode
	TMOD &= ~0x03; // Clear the timer 1 mode bits
	TMOD |= 0x10;  // Timer 1 in mode 1 (16-bit auto-reload)

	// Set Timer 1 to use SYSCLK as the clock source
	CKCON0 &= ~0x04; // Clear the T1M bit to select SYSCLK as the clock source

	// Calculate the reload value to generate a 2000 Hz signal
	// Reload Value = 65536 - (SYSCLK / (Prescaler * Desired Frequency))
	TH1 = 0xFF - ((SYSCLK / (TIMER1_PRESCALER * 2000)) >> 8);
	TL1 = 0xFF - ((SYSCLK / (TIMER1_PRESCALER * 2000)) & 0xFF);

	// Start Timer 1
	TR1 = 1;
}

/*void Buzzer_Init() {
	// Configure GPIO pin 3.1 as push-pull output
	P3MDOUT |= 0x02; // P3.1 as push-pull output
}*/

void UART1_Init(unsigned long baudrate)
{
	SFRPAGE = 0x20;
	SMOD1 = 0x0C; // no parity, 8 data bits, 1 stop bit
	SCON1 = 0x10;
	SBCON1 = 0x00; // disable baud rate generator
	SBRL1 = 0x10000L - ((SYSCLK / baudrate) / (12L * 2L));
	TI1 = 1;		// indicate ready for TX
	SBCON1 |= 0x40; // enable baud rate generator
	SFRPAGE = 0x00;
}

void putchar1(char c)
{
	SFRPAGE = 0x20;
	while (!TI1)
		;
	TI1 = 0;
	SBUF1 = c;
	SFRPAGE = 0x00;
}

void putnum1(int a)
{
	SFRPAGE = 0x20;
	while (!TI1)
		;
	TI1 = 0;
	SBUF1 = a;
	SFRPAGE = 0x00;
}

void sendstr1(char *s)
{
	while (*s)
	{
		putchar1(*s);
		s++;
	}
}

char getchar1(void)
{
	char c;
	SFRPAGE = 0x20;
	while (!RI1)
		;
	RI1 = 0;
	// Clear Overrun and Parity error flags
	SCON1 &= 0b_0011_1111;
	c = SBUF1;
	SFRPAGE = 0x00;
	return (c);
}

char getchar1_with_timeout(void)
{
	char c;
	unsigned int timeout;
	SFRPAGE = 0x20;
	timeout = 0;
	while (!RI1)
	{
		SFRPAGE = 0x00;
		Timer3us(20);
		SFRPAGE = 0x20;
		timeout++;
		if (timeout == 25000)
		{
			SFRPAGE = 0x00;
			return ('\n'); // Timeout after half second
		}
	}
	RI1 = 0;
	// Clear Overrun and Parity error flags
	SCON1 &= 0b_0011_1111;
	c = SBUF1;
	SFRPAGE = 0x00;
	return (c);
}

void getstr1(char *s)
{
	char c;

	while (1)
	{
		c = getchar1_with_timeout();
		if (c == '\n')
		{
			*s = 0;
			return;
		}
		*s = c;
		s++;
	}
}

// RXU1 returns '1' if there is a byte available in the receive buffer of UART1
bit RXU1(void)
{
	bit mybit;
	SFRPAGE = 0x20;
	mybit = RI1;
	SFRPAGE = 0x00;
	return mybit;
}

void waitms_or_RI1(unsigned int ms)
{
	unsigned int j;
	unsigned char k;
	for (j = 0; j < ms; j++)
	{
		for (k = 0; k < 4; k++)
		{
			if (RXU1())
				return;
			Timer3us(250);
		}
	}
}

void SendATCommand(char *s)
{
	printf("Command: %s", s);
	P2_1 = 0; // 'set' pin to 0 is 'AT' mode.
	waitms(5);
	sendstr1(s);
	getstr1(buffer);
	waitms(10);
	P2_1 = 1; // 'set' pin to 1 is normal operation mode.
	printf("Response: %s\r\n", buffer);
}

void InitADC(void)
{
	SFRPAGE = 0x00;
	ADEN = 0; // Disable ADC

	ADC0CN1 =
		(0x2 << 6) | // 0x0: 10-bit, 0x1: 12-bit, 0x2: 14-bit
		(0x0 << 3) | // 0x0: No shift. 0x1: Shift right 1 bit. 0x2: Shift right 2 bits. 0x3: Shift right 3 bits.
		(0x0 << 0);	 // Accumulate n conversions: 0x0: 1, 0x1:4, 0x2:8, 0x3:16, 0x4:32

	ADC0CF0 =
		((SYSCLK / SARCLK) << 3) | // SAR Clock Divider. Max is 18MHz. Fsarclk = (Fadcclk) / (ADSC + 1)
		(0x0 << 2);				   // 0:SYSCLK ADCCLK = SYSCLK. 1:HFOSC0 ADCCLK = HFOSC0.

	ADC0CF1 =
		(0 << 7) |	 // 0: Disable low power mode. 1: Enable low power mode.
		(0x1E << 0); // Conversion Tracking Time. Tadtk = ADTK / (Fsarclk)

	ADC0CN0 =
		(0x0 << 7) | // ADEN. 0: Disable ADC0. 1: Enable ADC0.
		(0x0 << 6) | // IPOEN. 0: Keep ADC powered on when ADEN is 1. 1: Power down when ADC is idle.
		(0x0 << 5) | // ADINT. Set by hardware upon completion of a data conversion. Must be cleared by firmware.
		(0x0 << 4) | // ADBUSY. Writing 1 to this bit initiates an ADC conversion when ADCM = 000. This bit should not be polled to indicate when a conversion is complete. Instead, the ADINT bit should be used when polling for conversion completion.
		(0x0 << 3) | // ADWINT. Set by hardware when the contents of ADC0H:ADC0L fall within the window specified by ADC0GTH:ADC0GTL and ADC0LTH:ADC0LTL. Can trigger an interrupt. Must be cleared by firmware.
		(0x0 << 2) | // ADGN (Gain Control). 0x0: PGA gain=1. 0x1: PGA gain=0.75. 0x2: PGA gain=0.5. 0x3: PGA gain=0.25.
		(0x0 << 0);	 // TEMPE. 0: Disable the Temperature Sensor. 1: Enable the Temperature Sensor.

	ADC0CF2 =
		(0x0 << 7) | // GNDSL. 0: reference is the GND pin. 1: reference is the AGND pin.
		(0x1 << 5) | // REFSL. 0x0: VREF pin (external or on-chip). 0x1: VDD pin. 0x2: 1.8V. 0x3: internal voltage reference.
		(0x1F << 0); // ADPWR. Power Up Delay Time. Tpwrtime = ((4 * (ADPWR + 1)) + 2) / (Fadcclk)

	ADC0CN2 =
		(0x0 << 7) | // PACEN. 0x0: The ADC accumulator is over-written.  0x1: The ADC accumulator adds to results.
		(0x0 << 0);	 // ADCM. 0x0: ADBUSY, 0x1: TIMER0, 0x2: TIMER2, 0x3: TIMER3, 0x4: CNVSTR, 0x5: CEX5, 0x6: TIMER4, 0x7: TIMER5, 0x8: CLU0, 0x9: CLU1, 0xA: CLU2, 0xB: CLU3

	ADEN = 1; // Enable ADC
}

#define VDD 3.3035 // The measured value of VDD in volts

void InitPinADC(unsigned char portno, unsigned char pin_num)
{
	unsigned char mask;

	mask = 1 << pin_num;

	SFRPAGE = 0x20;
	switch (portno)
	{
	case 0:
		P0MDIN &= (~mask); // Set pin as analog input
		P0SKIP |= mask;	   // Skip Crossbar decoding for this pin
		break;
	case 1:
		P1MDIN &= (~mask); // Set pin as analog input
		P1SKIP |= mask;	   // Skip Crossbar decoding for this pin
		break;
	case 2:
		P2MDIN &= (~mask); // Set pin as analog input
		P2SKIP |= mask;	   // Skip Crossbar decoding for this pin
		break;
	default:
		break;
	}
	SFRPAGE = 0x00;
}

unsigned int ADC_at_Pin(unsigned char pin)
{
	ADC0MX = pin; // Select input from pin
	ADINT = 0;
	ADBUSY = 1; // Convert voltage at the pin
	while (!ADINT)
		; // Wait for conversion to complete
	return (ADC0);
}

float Volts_at_Pin(unsigned char pin)
{
	return ((ADC_at_Pin(pin) * VDD) / 16383.0);
}

void main(void)
{

	float v[4];
	char volt1[12];

	char mpow = 'N';
	char mov_setting = 'X';
	// char shapes;

	int count = 0;

	float pwm1; // y or speed control
	float pwm2; // x or speed control

	unsigned int cnt;
	char pulsestring[16];

	unsigned long int x, f;

	waitms(500);
	printf("\r\nJDY-40 test\r\n");
	UART1_Init(9600);

	waitms(500); // Give PuTTy a chance to start before sending
	// printf("\x1b[2J"); // Clear screen using ANSI escape sequence.

	// printf ("ADC test program\n"
	//       "File: %s\n"
	//     "Compiled: %s, %s\n\n",
	//   __FILE__, __DATE__, __TIME__);

	InitPinADC(2, 2); // Configure P2.2 as analog input
	InitPinADC(2, 3); // Configure P2.3 as analog input
	InitPinADC(2, 4); // Configure P2.4 as analog input
	InitPinADC(2, 5); // Configure P2.5 as analog input
	InitADC();
	LCD_4BIT();

	LCDprint("LOL!!", 1, 1);
	LCDprint("LOL!!", 2, 1);

	// We should select an unique device ID.  The device ID can be a hex
	// number from 0x0000 to 0xFFFF.  In this case is set to 0xABBA
	SendATCommand("AT+DVIDACCA\r\n"); // changed from ABBA to ACCA

	// To check configuration
	SendATCommand("AT+VER\r\n");
	SendATCommand("AT+BAUD\r\n");
	SendATCommand("AT+RFID\r\n");
	SendATCommand("AT+DVID\r\n");
	SendATCommand("AT+RFC\r\n");
	SendATCommand("AT+POWE\r\n");
	SendATCommand("AT+CLSS\r\n");
	printf("\r\Press and hold the BOOT button to transmit.\r\n");

	cnt = 0;

	printf("\x1b[2J"); // Clear screen using ANSI escape sequence.
	printf("Variable frequency generator for the EFM8LB1.\r\n"
		   "Check pins P2.0 and P2.1 with the oscilloscope.\r\n");

	/*	printf("New frequency=");
		scanf("%lu", &f);
		x=(SYSCLK/(2L*f));

		if(x>0xffff)
		{
			printf("Sorry %lu Hz is out of range.\n", f);
		}
		else
		{
			TR2=0; // Stop timer 2
			TMR2RL=0x10000L-x; // Change reload value for new frequency
			TR2=1; // Start timer 2
			f=SYSCLK/(2L*(0x10000L-TMR2RL));
			printf("\nActual frequency: %lu\n", f);
		}*/
		

	while (1)
	{

		char movement;
		// char receiving = 'N';
		// char metal_strength[20];

		// sprintf(buffer, "%d %d\r\n", (int)pwm1, (int)pwm2);
		// sendstr1(buffer);
		//  putchar('.');
		printf(".");
		// waitms_or_RI1(5);
		//  }

		/*if (RXU1())
		{
			getstr1(buffer);
			printf("Metal Strength: %s", buffer);
		}*/

		// Read 14-bit value from the pins configured as analog inputs
		v[1] = Volts_at_Pin(QFP32_MUX_P2_3);
		v[2] = Volts_at_Pin(QFP32_MUX_P2_4);

		// sprintf(volt1, "%.2f %.2f", v[1], v[2]);

		pwm1 = (1.60 - v[1]) * 100 / 3.3 + 51;
		pwm2 = (v[2] * 100) / 3.35;

		if (pwm2 == 0)
		{

			pwm2 = 2;
		}

		if (pwm1 == 0)
		{

			pwm1 = 2;
		}

		if ((pwm1 == 50) && (pwm2 > 50))
		{

			movement = 'A';
		}

		else if (pwm1 >= 50 && pwm2 >= 50)
		{

			movement = 'B';
		}

		else if (pwm1 > 50 && pwm2 == 50)
		{

			movement = 'C';
		}

		else if (pwm1 > 50 && pwm2 < 50)
		{

			movement = 'D';
		}

		else if (pwm1 == 50 && pwm2 < 50)
		{

			movement = 'E';
		}

		else if (pwm1 < 50 && pwm2 < 50)
		{

			movement = 'F';
		}

		else if (pwm1 < 50 && pwm2 == 50)
		{

			movement = 'G';
		}

		else if (pwm1 <= 50 && pwm2 >= 50)
		{

			movement = 'H';
		}

		else
		{

			movement = 'W';
		}

		if (count == 10)
		{

			movement = 'S';
		}

		if (count > 10)
		{

			waitms(1);
		}

		while (count > 10 && count < 19)
		{

			if (RXU1())
			{

				getstr1(buffer);

				if (buffer[0] == 'N' || buffer[0] == 'K' || buffer[0] == 'O' || buffer[0] == 'P' || buffer[0] == 'L' || buffer[0] == 'M')
				{

					printf("Metal Strength: %s", buffer);
					mpow = buffer[0];
					count = 0;
				}
			}
			// printf("break");
			count++;
		}
		
		//sprintf(volt1, "%s", mpow);

		if (mpow == 'N' && bflag != 1)
		{

			sprintf(volt1, "Neutral");
			buzz = 0;
			
			/*f = 4096;
			buzz = 500;
			x = (SYSCLK / (2L * f));
			TR2 = 0;			   // Stop timer 2
			TMR2RL = 0x10000L - x; // Change reload value for new frequency
			TR2 = 1;			   // Start timer 2
			f = SYSCLK / (2L * (0x10000L - TMR2RL));*/
			bflag = 1;
		}

		else if (mpow == 'M' && bflag != 2)
		{
			sprintf(volt1, "Small");
			f = 3300;
			buzz = 200;
			x = (SYSCLK / (2L * f));
			TR2 = 0;			   // Stop timer 2
			TMR2RL = 0x10000L - x; // Change reload value for new frequency
			TR2 = 1;			   // Start timer 2
			f = SYSCLK / (2L * (0x10000L - TMR2RL));
			bflag = 2;
		}

		else if (mpow == 'O' && bflag != 3)
		{
			sprintf(volt1, "Medium");
			f = 3500;
			buzz = 300;
			x = (SYSCLK / (2L * f));
			TR2 = 0;			   // Stop timer 2
			TMR2RL = 0x10000L - x; // Change reload value for new frequency
			TR2 = 1;			   // Start timer 2
			f = SYSCLK / (2L * (0x10000L - TMR2RL));
			bflag = 3;
		}

		else if (mpow == 'K' && bflag != 4)
		{

			sprintf(volt1, "Strong");
			f = 3700;
			buzz = 400;
			x = (SYSCLK / (2L * f));
			TR2 = 0;			   // Stop timer 2
			TMR2RL = 0x10000L - x; // Change reload value for new frequency
			TR2 = 1;			   // Start timer 2
			f = SYSCLK / (2L * (0x10000L - TMR2RL));
			bflag = 4;
		}

		else if (mpow == 'L' && bflag != 5)
		{

			sprintf(volt1, "XStrong");
			f = 3900;
			buzz = 500;
			x = (SYSCLK / (2L * f));
			TR2 = 0;			   // Stop timer 2
			TMR2RL = 0x10000L - x; // Change reload value for new frequency
			TR2 = 1;			   // Start timer 2
			f = SYSCLK / (2L * (0x10000L - TMR2RL));
			bflag = 5;
		}
		else if (mpow == 'P' && bflag != 6)
		{

			sprintf(volt1, "SSSSStrong");

			f = 4096;
			buzz = 600;
			x = (SYSCLK / (2L * f));
			TR2 = 0;			   // Stop timer 2
			TMR2RL = 0x10000L - x; // Change reload value for new frequency
			TR2 = 1;			   // Start timer 2
			f = SYSCLK / (2L * (0x10000L - TMR2RL));
			bflag = 6;

			printf("\nActual frequency: %lu\n", f);
		}

		if (count >= 16)
		{
			count = 0;
		}

		if (pb_setting == 0 && mov_setting == 'X')
		{
			while (pb_setting == 0)
			{
			}

			mov_setting = 'Y';
		}
		else if (pb_setting == 0 && mov_setting == 'Y')
		{
			while (pb_setting == 0)
			{
			}

			mov_setting = 'Z';
		}
		else if (pb_setting == 0 && mov_setting == 'Z')
		{
			while (pb_setting == 0)
			{
			}

			mov_setting = 'X';
		}

		if (P0_3 == 0)
		{
			movement = mov_setting;
		}

		sprintf(buffer, "%c %02d %02d\r\n", movement, (int)pwm1, (int)pwm2);
		sendstr1(buffer);

		waitms_or_RI1(5);



		if (mov_setting == 'X')
		{	
		
			sprintf(pulsestring, "%c %02d %02d S:| %d", movement, (int)pwm1, (int)pwm2,f);

		}
		else if (mov_setting == 'Y')
		{
		
			sprintf(pulsestring, "%c %02d %02d S:O %d", movement, (int)pwm1, (int)pwm2,f);

		}
		else if (mov_setting == 'Z')
		{
			sprintf(pulsestring, "%c %02d %02d S:8 %d", movement, (int)pwm1, (int)pwm2,f);

		}
		
		

		// printf ("X=%7.5fV, Y=%7.5fV   XPOW=%f YPOW=%f \r", v[2], v[1],pwm2, pwm1);
		// waitms(5);
		
		LCDprint(pulsestring, 1, 1);
		LCDprint(volt1, 2, 1);
		

		

		/*x=(SYSCLK/(2L*f));
		TR2=0; // Stop timer 2
		TMR2RL=0x10000L-x; // Change reload value for new frequency
		TR2=1; // Start timer 2
		f=SYSCLK/(2L*(0x10000L-TMR2RL));*/

		count++;
	}
}