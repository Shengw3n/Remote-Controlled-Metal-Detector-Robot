#include "../Common/Include/stm32l051xx.h"
#include <stdio.h>
#include <stdlib.h>
#include "../Common/Include/serial.h"
#include "UART2.h"
#include <math.h>

#define SYSCLK 32000000L
#define DEF_F 15000L
#define PIN_PERIOD (GPIOA->IDR & BIT8)
#define F_CPU 32000000L

volatile int PWM_Counter = 0;
volatile int pwm1 = 0, pwm2 = 0, pwm3 = 0, pwm4 = 0;

// LQFP32 pinout
//             ----------
//       VDD -|1       32|- VSS
//      PC14 -|2       31|- BOOT0
//      PC15 -|3       30|- PB7
//      NRST -|4       29|- PB6
//      VDDA -|5       28|- PB5
//       PA0 -|6       27|- PB4
//       PA1 -|7       26|- PB3
//       PA2 -|8       25|- PA15 (Used for RXD of UART2, connects to TXD of JDY40)
//       PA3 -|9       24|- PA14 (Used for TXD of UART2, connects to RXD of JDY40)
//       PA4 -|10      23|- PA13 (Used for SET of JDY40)
//       PA5 -|11      22|- PA12
//       PA6 -|12      21|- PA11
//       PA7 -|13      20|- PA10 (Reserved for RXD of UART1)
//       PB0 -|14      19|- PA9  (Reserved for TXD of UART1)
//       PB1 -|15      18|- PA8  (pushbutton)
//       VSS -|16      17|- VDD
//             ----------

void delay(int dly)
{
	while (dly--)
		;
}

long int GetPeriod(int n)
{
	//__disable_irq();

	int i;
	unsigned int saved_TCNT1a, saved_TCNT1b;

	SysTick->LOAD = 0xffffff;											  // 24-bit counter set to check for signal present
	SysTick->VAL = 0xffffff;											  // load the SysTick counter
	SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk; // Enable SysTick IRQ and SysTick Timer */
	while (PIN_PERIOD != 0)												  // Wait for square wave to be 0
	{
		if (SysTick->CTRL & BIT16)
			return 0;
	}
	SysTick->CTRL = 0x00; // Disable Systick counter

	SysTick->LOAD = 0xffffff;											  // 24-bit counter set to check for signal present
	SysTick->VAL = 0xffffff;											  // load the SysTick counter
	SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk; // Enable SysTick IRQ and SysTick Timer */
	while (PIN_PERIOD == 0)												  // Wait for square wave to be 1
	{
		if (SysTick->CTRL & BIT16)
			return 0;
	}
	SysTick->CTRL = 0x00; // Disable Systick counter

	SysTick->LOAD = 0xffffff;											  // 24-bit counter reset
	SysTick->VAL = 0xffffff;											  // load the SysTick counter to initial value
	SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk; // Enable SysTick IRQ and SysTick Timer */
	for (i = 0; i < n; i++)												  // Measure the time of 'n' periods
	{
		while (PIN_PERIOD != 0) // Wait for square wave to be 0
		{
			if (SysTick->CTRL & BIT16)
				return 0;
		}
		while (PIN_PERIOD == 0) // Wait for square wave to be 1
		{
			if (SysTick->CTRL & BIT16)
				return 0;
		}
	}
	SysTick->CTRL = 0x00; // Disable Systick counter
	//__enable_irq();

	return 0xffffff - SysTick->VAL;
}

// Uses SysTick to delay <us> micro-seconds.
void Delay_us(unsigned char us)
{
	// For SysTick info check the STM32L0xxx Cortex-M0 programming manual page 85.
	SysTick->LOAD = (F_CPU / (1000000L / us)) - 1;						  // set reload register, counter rolls over from zero, hence -1
	SysTick->VAL = 0;													  // load the SysTick counter
	SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk; // Enable SysTick IRQ and SysTick Timer */
	while ((SysTick->CTRL & BIT16) == 0)
		;				  // Bit 16 is the COUNTFLAG.  True when counter rolls over from zero.
	SysTick->CTRL = 0x00; // Disable Systick counter
}

/*void waitms(unsigned int ms)
{
	unsigned int j;
	unsigned char k;
	for (j = 0; j < ms; j++)
		for (k = 0; k < 4; k++)
			Delay_us(250);
}*/

void wait_1ms(void)
{
	// For SysTick info check the STM32L0xxx Cortex-M0 programming manual page 85.
	SysTick->LOAD = (F_CPU / 1000L) - 1;								  // set reload register, counter rolls over from zero, hence -1
	SysTick->VAL = 0;													  // load the SysTick counter
	SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk; // Enable SysTick IRQ and SysTick Timer */
	while ((SysTick->CTRL & BIT16) == 0)
		;				  // Bit 16 is the COUNTFLAG.  True when counter rolls over from zero.
	SysTick->CTRL = 0x00; // Disable Systick counter
}

void waitms(int len)
{
	while (len--)
		wait_1ms();
}

// Interrupt service routines are the same as normal
// subroutines (or C funtions) in Cortex-M microcontrollers.
// The following should happen at a rate of 1kHz.
// The following function is associated with the TIM2 interrupt
// via the interrupt vector table defined in startup.c
void TIM2_Handler(void)
{
	TIM2->SR &= ~BIT0; // clear update interrupt flag
	PWM_Counter++;

	// forward
	if (pwm1 > PWM_Counter)
	{
		GPIOA->ODR |= BIT2;
	}
	else
	{
		GPIOA->ODR &= ~BIT2;
	}

	if (pwm2 > PWM_Counter)
	{
		GPIOA->ODR |= BIT4;
	}
	else
	{
		GPIOA->ODR &= ~BIT4;
	}

	// backward
	if (pwm3 > PWM_Counter)
	{
		GPIOA->ODR |= BIT3;
	}
	else
	{
		GPIOA->ODR &= ~BIT3;
	}

	if (pwm4 > PWM_Counter)
	{
		GPIOA->ODR |= BIT5;
	}
	else
	{
		GPIOA->ODR &= ~BIT5;
	}

	if (PWM_Counter > 100) // THe period is 20ms
	{
		PWM_Counter = 0;
		GPIOA->ODR |= (BIT2 | BIT4);
	}
}

void Hardware_Init(void)
{
	// Set up output pins
	RCC->IOPENR |= BIT0;									  // peripheral clock enable for port A
	GPIOA->MODER = (GPIOA->MODER & ~(BIT22 | BIT23)) | BIT22; // Make pin PA11 output (page 200 of RM0451, two bits used to configure: bit0=1, bit1=0)
	GPIOA->OTYPER &= ~BIT11;								  // Push-pull
	GPIOA->MODER = (GPIOA->MODER & ~(BIT24 | BIT25)) | BIT24; // Make pin PA12 output (page 200 of RM0451, two bits used to configure: bit0=1, bit1=0)
	GPIOA->OTYPER &= ~BIT12;								  // Push-pull

	// Make pin PA1 output
	GPIOA->MODER = (GPIOA->MODER & ~(BIT2 | BIT3)) | BIT2; // Reset bit 3, Set bit 2
	GPIOA->OTYPER &= ~BIT1;								   // Configure pin 1 as push-pull (default)

	// Make pin PA2 output
	GPIOA->MODER = (GPIOA->MODER & ~(BIT4 | BIT5)) | BIT4; // Reset bit 5, Set bit 4
	GPIOA->OTYPER &= ~BIT2;								   // Push-pull

	// Make pin PA3 output
	GPIOA->MODER = (GPIOA->MODER & ~(BIT6 | BIT7)) | BIT6; // Reset bit 7, Set bit 6
	GPIOA->OTYPER &= ~BIT3;								   // Push-pull

	// Make pin PA4 output
	GPIOA->MODER = (GPIOA->MODER & ~(BIT8 | BIT9)) | BIT8; // Reset bit 9, Set bit 8
	GPIOA->OTYPER &= ~BIT4;								   // Push-pull

	// Make pin PA5 output
	GPIOA->MODER = (GPIOA->MODER & ~(BIT10 | BIT11)) | BIT10; // Reset bit 11, Set bit 10
	GPIOA->OTYPER &= ~BIT5;									  // Push-pull

	GPIOA->OSPEEDR = 0xffffffff; // All pins of port A configured for very high speed! Page 201 of RM0451

	RCC->IOPENR |= BIT0; // peripheral clock enable for port A

	GPIOA->MODER = (GPIOA->MODER & ~(BIT27 | BIT26)) | BIT26; // Make pin PA13 output (page 200 of RM0451, two bits used to configure: bit0=1, bit1=0))
	GPIOA->ODR |= BIT13;									  // 'set' pin to 1 is normal operation mode.

	// GPIOA->ODR |= BIT2; // Set PA2 high
	GPIOA->ODR &= ~BIT2;

	// Set up timer
	RCC->APB1ENR |= BIT0; // turn on clock for timer2 (UM: page 177)
	TIM2->ARR = F_CPU / DEF_F - 1;
	NVIC->ISER[0] |= BIT15; // enable timer 2 interrupts in the NVIC
	TIM2->CR1 |= BIT4;		// Downcounting
	TIM2->CR1 |= BIT7;		// ARPE enable
	TIM2->DIER |= BIT0;		// enable update event (reload event) interrupt
	TIM2->CR1 |= BIT0;		// enable counting

	__enable_irq();
}

void SendATCommand(char *s)
{
	char buff[40];
	printf("Command: %s", s);
	GPIOA->ODR &= ~(BIT13); // 'set' pin to 0 is 'AT' mode.
	waitms(10);
	eputs2(s);
	egets2(buff, sizeof(buff) - 1);
	GPIOA->ODR |= BIT13; // 'set' pin to 1 is normal operation mode.
	waitms(10);
	printf("Response: %s", buff);
}

int main(void)
{
	char buff[80];
	int cnt = 0;
	char *token;
	int numbers[2]; // Array to hold the two integers
	int index = 0;
	int last_y = 50;
	int last_x = 50;
	int y_value;
	int x_value;
	int avg_count = 1;
	int avg;
	int sum = 0;
	char detect;
	int tcount = 0;
	int neutral_freq;

	char transmission_string[20];

	int yup;
	int ydown;
	int xright;
	int xleft;

	long int count;
	float T, f;

	RCC->IOPENR |= 0x00000001; // peripheral clock enable for port A

	GPIOA->MODER &= ~(BIT16 | BIT17); // Make pin PA8 input
	// Activate pull up for pin PA8:
	GPIOA->PUPDR |= BIT16;
	GPIOA->PUPDR &= ~(BIT17);

	waitms(500); // Wait for putty to start.
	printf("Period measurement using the Systick free running counter.\r\n"
		   "Connect signal to PA8 (pin 18).\r\n");

	Hardware_Init();
	initUART2(9600);

	waitms(1000); // Give putty some time to start.
	printf("\r\nJDY-40 test\r\n");

	// We should select an unique device ID.  The device ID can be a hex
	// number from 0x0000 to 0xFFFF.  In this case is set to 0xABBA
	SendATCommand("AT+DVIDACCA\r\n"); // ABCA from ACCA

	// To check configuration
	SendATCommand("AT+VER\r\n");
	SendATCommand("AT+BAUD\r\n");
	SendATCommand("AT+RFID\r\n");
	SendATCommand("AT+DVID\r\n");
	SendATCommand("AT+RFC\r\n");
	SendATCommand("AT+POWE\r\n");
	SendATCommand("AT+CLSS\r\n");

	printf("\r\nPress and hold a push-button attached to PA8 (pin 18) to transmit.\r\n");

	cnt = 0;

	while (1)
	{
		char movement;

		if (avg_count >= 450)
		{

			avg_count = 1;
			sum = 0;
		}
		count = GetPeriod(100);
		if (count > 0)
		{
			T = count / (F_CPU * 100.0); // Since we have the time of 100 periods, we need to divide by 100
			f = 1.0 / T;
			sum += f;
			avg = sum / avg_count;
			// printf("f=%dHz, count=%d   average=%d         \r", (int)f, count, avg);
			avg_count++;
		}
		else
		{

			printf("NO SIGNAL \r");
		}
		fflush(stdout); // GCC printf wants a \n in order to send something.  If \n is not present, we fflush(stdout)
						// waitms(20);

		/*if ((GPIOA->IDR & BIT8) == 0)
		{
			sprintf(buff, "JDY40 test %d\r\n", cnt++);
			eputs2(buff);
			printf(".");
			waitms(200);
		}*/

		if ((ReceivedBytes2() > 0))
		{ // Something has arrived

			egets2(buff, sizeof(buff) - 1);

			// printf("%s\r\n", buff);

			movement = buff[0];

			y_value = atoi(&buff[2]); // num1 is now 89

			x_value = atoi(&buff[5]); // num2 is now 137 // printf("The first integer is: %d\n\r", a); // printf("The second integer is: %d\n\r", c); // printf("%s", buff[0]);

			// printf("%c %d %d f=%dHz, count=%d   average=%d %c \r", movement,  y_value,x_value, (int)f, avg_count, avg, detect);

			if (movement == 'S')
			{

				x_value = last_x;
				y_value = last_y;

				waitms(3);

				while (tcount < 5)
				{

					sprintf(transmission_string, "%c \r\n", detect);
					eputs2(transmission_string);
					tcount++;
				}

				tcount = 0;
			}
		}

		/*	else{

				sprintf(buff, "JDY40 test\r\n");
				eputs2(buff);

			}*/

		// Turns on Green LED on if JDY resetting is not needed
		if (movement == 'B' || movement == 'D' || movement == 'F' || movement == 'H' || movement == 'W' || movement == 'A' || movement == 'C' || movement == 'S')
		{
			GPIOA->ODR |= BIT1;
		}

		if (movement == 'X')
		{

			pwm1 = 29;
			pwm2 = 30;

			waitms(3000);

			pwm1 = 68;
			pwm2 = 70;

			waitms(3000);

			pwm1 = 94;
			pwm2 = 100;

			waitms(3000);

			pwm1 = 0;
			pwm2 = 0;

			waitms(1000);

			pwm3 = 30;
			pwm4 = 30;

			waitms(3000);

			pwm3 = 70;
			pwm4 = 70;

			waitms(3000);

			pwm3 = 100;
			pwm4 = 100;

			waitms(3000);

			pwm3 = 0;
			pwm4 = 0;
		}

		if (movement == 'Y')
		{

			for (int ycounter = 0; ycounter < 4; ycounter++)
			{

				pwm1 = 96;
				pwm2 = 100;

				waitms(2000);

				pwm1 = 0;
				pwm2 = 100;

				waitms(1175);
			}

			pwm1 = 0;
			pwm2 = 0;
		}

		if (movement == 'Z')
		{

			pwm1 = 100;
			pwm2 = 50;

			waitms(8750);

			pwm1 = 50;
			pwm2 = 100;

			waitms(8750);

			pwm1 = 0;
			pwm2 = 0;

			waitms(1000);
		}

		if (y_value == 50 && x_value > 50 && movement == 'A' && y_value < 101 && x_value < 101)
		{

			last_x = x_value;
			last_y = y_value;
		}

		else if (y_value >= 50 && x_value >= 50 && movement == 'B' && y_value < 101 && x_value < 101)
		{

			last_x = x_value;
			last_y = y_value;
		}

		else if (y_value > 50 && x_value == 50 && movement == 'C' && y_value < 101 && x_value < 101)
		{

			last_x = x_value;
			last_y = y_value;
		}

		else if (y_value >= 50 && x_value <= 50 && movement == 'D' && y_value < 101 && x_value < 101)
		{

			if ((x_value == 9 || x_value == 4 || x_value == 0 || x_value == 1) && (last_x == 49 || last_x == 50))
			{

				y_value = last_y;
				x_value = last_x;
			}

			else
			{

				last_x = x_value;
				last_y = y_value;
			}
		}

		else if (y_value == 50 && x_value < 50 && movement == 'E' && y_value < 101 && x_value < 101)
		{

			last_x = x_value;
			last_y = y_value;
		}

		else if (y_value < 50 && x_value < 50 && movement == 'F' && y_value < 101 && x_value < 101)
		{

			if ((x_value == 9 || x_value == 4 || x_value == 0 || x_value == 1) && (last_x == 49 || last_x == 50))
			{

				y_value = last_y;
				x_value = last_x;
			}

			else
			{

				last_x = x_value;
				last_y = y_value;
			}
		}

		else if (y_value < 50 && x_value == 50 && movement == 'G' && y_value < 101 && x_value < 101)
		{

			last_x = x_value;
			last_y = y_value;
		}

		else if (y_value <= 50 && x_value >= 50 && movement == 'H' && y_value < 101 && x_value < 101)
		{

			last_x = x_value;
			last_y = y_value;
		}

		else if (movement = 'S')
		{

			x_value = last_x;
			y_value = last_y;
		}

		else
		{

			y_value = last_y;
			x_value = last_x;
		}

		printf("%c %d %d %d %d %d %d %c\r\n", movement, y_value, x_value, xright, xleft, pwm1, pwm2, detect);

		neutral_freq = 24069;
		if (avg - neutral_freq < 20)
		{

			detect = 'N';
		}

		if (avg - neutral_freq >= 20 && avg - neutral_freq <= 50)
		{

			detect = 'M';
		}

		else if (avg - neutral_freq > 50 && avg - neutral_freq <= 90)
		{

			detect = 'O';
		}

		else if (avg - neutral_freq > 90 && avg - neutral_freq <= 130)
		{

			detect = 'K';
		}

		else if (avg - neutral_freq > 130 && avg - neutral_freq <= 190)
		{

			detect = 'L';
		}
		else if (avg - neutral_freq > 190)
		{

			detect = 'P';
		}

		// printf("%c %d %d f=%dHz, count=%d   average=%d %c \r", movement, y_value, x_value, (int)f, avg_count, avg, detect);

		yup = (y_value - 50) * 2;
		ydown = (100 - 2 * y_value);
		xright = (x_value - 50) * 2;
		xleft = (100 - 2 * x_value);

		// pivoting right
		if (y_value <= 55 && y_value >= 45 && x_value >= 50)
		{

			pwm1 = 0;
			pwm2 = xright; // 1.2*yup/(1.2*yup+0.8*xleft+1)*100;
			pwm3 = 0;
			pwm4 = 0;
		}

		// pivoting left
		if (y_value <= 55 && y_value >= 45 && x_value < 50)
		{

			pwm1 = xleft;
			pwm2 = 0; // 1.2*yup/(1.2*yup+0.8*xleft+1)*100;
			pwm3 = 0;
			pwm4 = 0;
		}

		// forward right
		if (y_value > 55 && x_value >= 50)
		{

			pwm1 = yup - 0.5 * xright;
			pwm2 = yup; // 1.2*yup/(1.2*yup+0.8*xleft+1)*100;
			pwm3 = 0;
			pwm4 = 0;
		}
		// forward left
		if (y_value > 55 && x_value <= 50)
		{

			pwm2 = yup - 0.5 * xleft + 2;
			pwm1 = yup - 3; // 1.2*yup/(1.2*yup+0.8*xright+1)*100;
			pwm3 = 0;
			pwm4 = 0; // xleft;
		}

		// backward right
		if (y_value < 45 && x_value >= 50)
		{

			pwm3 = ydown - 0.5 * xright;
			pwm4 = ydown;
			pwm2 = 0;
			pwm1 = 0;
		}
		// backward left
		if (y_value < 45 && x_value < 50)
		{

			pwm3 = ydown;
			pwm4 = ydown - 0.5 * xleft;
			pwm2 = 0;
			pwm1 = 0;
		}
	}
}