#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <util/delay.h>
#include "lcd.h"
#include "uart.h"

#define FREQ 16000000
#define BAUD 9600
#define HIGH 1
#define LOW 0
#define BUFFER 1024
#define BLACK 0x000001

char displayChar = 0;
int xcor=0,ycor=0; // Touchscreen coordinates
int xd=0,yd=0; //LCD coordinates
int p1=0,p2=0; //player 1 and 2 
int xb=0,yb=0; // Ball center coordinates
int flag1=0; // Condition for restart round check
int dx=5,dy; // velocities
int xp1=0,yp1=0; // paddle 1 position
int xp2=0,yp2=0; // paddle 2 position
int yda=0; //paddle from accelerometer
int b=0; // BUzzer condition
int buz=0; // buzzer counter


//------------------------ADC Touchscreen----------------------------------------------

//ADC initialization for touchscreen
void adc_init()
{
	ADCSRA |= ((1 <<ADPS2)|(1 << ADPS1)|(1 << ADPS0)); // Prescaler at 128
	ADMUX |= (1 << REFS0);
	ADMUX &= ~(1 << REFS1); // AVcc (5V) as reference voltage
	//ADCSRA |= (1 << ADATE); // ADC Auto trigger enable
	//ADCSRB &= ~((1 << ADTS2)|(1 << ADTS1)|(1 << ADTS0)); //free runnning mode
	ADCSRA |= (1 << ADEN); // Power up the ADC
	//ADCSRA |= (1 << ADIE);//ADC interrupt enable
	//ADCSRA |= (1 << ADSC); // Start conversion
	//sei(); //global interrupt
}

//touchscreen initialization & value reading
void touch_screen_read()
{
	/*DDRC &= ~(1<<0);
	DDRC |= (1<<1);
	PORTC |= (1<<0); // standby mode - Pulled up Y-
	PORTC &= ~(1<<1); // standby mode
	ADMUX &= ~((1<<0)|(1<<1)|(1<<2)|(1<<3));
	if (PINC & (0<<0))
	{}*/
	
	// x axis read
	ADMUX &= ~((1<<0)|(1<<1)|(1<<2)|(1<<3));
	DDRC |= (1<<1)|(1<<3);
	DDRC &= ~(1<<2);
	DDRC &= ~(1<<0);
	PORTC &= ~(1<<1);
	PORTC |= (1<<3);
	ADCSRA |= (1 << ADSC);
	while ((ADCSRA & (1 << ADSC)));
	xcor = ADC;
	
	//y axis read
	ADMUX |= ((1<<0)|(1<<1));
	ADMUX &= ~((1<<2)|(1<<3));
	DDRC |= (1<<2)|(1<<0);
	DDRC &= ~(1<<1);
	DDRC &= ~(1<<3);
	PORTC &= ~(1<<2);
	PORTC |= (1<<0);
	ADCSRA |= (1 << ADSC);
	while ((ADCSRA & (1 << ADSC)));
	ycor = ADC;
	//printf ("The ADC output is xcor= %d & ycor= %d \n",xcor,ycor);
}

//Touchscreen and LCD coordinates calculation
void pixelcalc()
{
	if(ycor < 590)
	{
	xd = (-0.183)*xcor + (0.0019)*ycor + 165;
	yd = (-0.00145)*xcor + (0.147)*ycor - 23;	
	}
	//printf ("xd= %d & yd= %d \n",xd,yd);
}


//Display pixel on touch
void touchpixel()
{
	drawrect(buff,xd-2,yd-2,xd+2,yd+2,0x00);
	write_buffer(buff);	
}


//-------------------------------------------------------------------------------------



//----------------------------------Accelerometer ADC-----------------------

//ADC initialization for accelerometer
void adc_init_accelo()
{
	ADCSRA |= ((1 <<ADPS2)|(1 << ADPS1)|(1 << ADPS0)); // Prescaler at 128
	ADMUX |= (1 << REFS0);
	ADMUX &= ~(1 << REFS1); // AVcc (5V) as reference voltage
	//ADCSRA |= (1 << ADATE); // ADC Auto trigger enable
	//ADCSRB &= ~((1 << ADTS2)|(1 << ADTS1)|(1 << ADTS0)); //free runnning mode
	ADCSRA |= (1 << ADEN); // Power up the ADC
	//ADCSRA |= (1 << ADIE);//ADC interrupt enable
	//ADCSRA |= (1 << ADSC); // Start conversion
	//sei(); //global interrupt
}

//Accelerometer initialization & value reading
void accelo_read()
{
	//x axis variation read
	ADMUX &= ~((1<<0)|(1<<1)|(1<<3));
	ADMUX |= (1<<2);
	DDRC &= ~(1<<4);
	ADCSRA |= (1 << ADSC);
	while ((ADCSRA & (1 << ADSC)));
	xcor = ADC;
	//printf ("The ADC output is ycor= %d \n",xcor);
}

//Accelerometer and LCD coordinates calculation
void accelo_pixelcalc()
{
	yda = (350-xcor)*(63/60);
	
//printf ("yda= %d \n",yda);
}
//---------------------------------------------------------------------------------------

//---------------------GAME DYNAMICS---------------------------------------------------------

//Random number generation for velocity vector
void randomz()
{
	// y velocity and direction randomness
	int YArray[6] = {-5,-3,-2,2,3,5};
	int randomIndex1 = rand() % 6;
	dy = YArray[randomIndex1];

	// x direction randomness (avoided to save computation)
	//int XArray[2] = {-1,1};
	//int randomIndex2 = rand() % 2;
	//dx = XArray[randomIndex2];
	dx = dx*(-1);
}

//Game background display
void gamebg()
{
	uint8_t m;
	drawrect(buff,0,0,127,63,BLACK); //rectangle
	for (m=0;m<64;m++)
	{
		if ((m == 4)|(m == 12)|(m == 20)|(m == 28)||(m == 36)|(m == 44)|(m == 52)|(m == 60))
		{
			drawline(buff,64,m-4,64,m,0x00); // draw line
		}
	}
	drawstring(buff,15,0,"PING");
	drawstring(buff,90,0,"PONG");
	write_buffer(buff);
}

//--------------------------------------------Ball dynamics--------------------------------------------
void ball()
{
	//int dx,dy;
	//printf("The flag is %d, p1 = %d, p2 = %d , xb = %d, yb = %d, dx = %d, dy = %d, p1=%d , p2=%d \n",flag1,p1,p2,xb,yb,dx,dy,p1,p2);
	if (flag1 == 0) // if condition satisfied to start round 
	{	
		xb = 64;
		yb = 32; // @center of game 
		fillcircle(buff,xb,yb,2,0x00);
		write_buffer(buff);
		flag1++;
		_delay_ms(10000);
		randomz();
	}
	else
	{
		xb=xb+dx;
		yb=yb+dy;
		
		int yb1 = yb+2;
		int yb2 = yb-2;
		int xb1 = xb+2;
		int xb2 = xb-2;
		
		// wall interaction
		if (yb1 >= 60 || yb2 <= 3)
		{
			dy = dy*(-1); // horizontal wall
			b=1;
		}
		
		if (xb >= 124) // player 2 loses
		{
			flag1 = 0;
			p1++;
			b=3;
			PORTD |= (1<<7);
			_delay_ms(5000);
			PORTD &= ~(1<<7);
		}
		
		if (xb <= 5) //player 1 loses 
		{
			flag1 = 0;
			p2++;
			b=3;
			PORTB |= (1<<0);
			_delay_ms(5000);
			PORTB &= ~(1<<0);
		}
		
		// paddle interaction
		if (xb <= 9 && (yb>yp1-6 && yb<yp1+6))		
		{
			dx=dx*(-1);//paddle hit
			b=2;
		}
		else if (xb >= 115 && (yb>yp2-6 && yb<yp2+6))
		{
			dx=dx*(-1);//paddle hit	
			b=2;
		}
	}
		//Ball draw
		fillcircle(buff,xb,yb,2,0x00);
		write_buffer(buff);
		_delay_ms(1000);
}


//-----------------------------------Paddle dynamics-----------------------------------------------
void paddle()
{
if (flag1 == 0 && p1==0 && p2==0)
{
	xp1 = 1;
	yp1 = 32;
	xp2 = 126;
	yp2 = 32;
}
else if (xd < 64) // player 1 on the left part of screen
{
	if (yd <= 5)
	{
		xp1 = 1;
		yp1 = 5;
	}
	else if (yd >= 59)
	{
		xp1 = 1;
		yp1 = 58;
	}
	else
	{
		xp1 = 1;
		yp1 = yd;
	}
	
}
else if (xd > 64) // player 2 on right part of the screen
{
	if (yd <= 5)
	{
		xp2 = 126;
		yp2 = 5;
	}
	else if (yd >= 59)
	{
		xp2 = 126;
		yp2 = 58;
	}
	else
	{
		xp2 = 126;
		yp2 = yd;
	}
}
drawrect(buff,xp1,yp1-4,xp1+1,yp1+4,0x00);
write_buffer(buff);
drawrect(buff,xp2-1,yp2-4,xp2,yp2+4,0x00);
write_buffer(buff);
}

//------------------------------------Paddle dynamics -AI-------------------------------------
void paddleplayer()
{
	if (flag1 == 0 && p1==0 && p2==0)
	{
		xp1 = 1;
		yp1 = 32;
		xp2 = 126;
		yp2 = 32;
	}
	// player 1 on the left part of screen

		if (yda <= 5)
		{
			xp1 = 1;
			yp1 = 5;
		}
		else if (yda >= 59)
		{
			xp1 = 1;
			yp1 = 58;
		}
		else
		{
			xp1 = 1;
			yp1 = yda;
		}
		drawrect(buff,xp1,yp1-4,xp1+1,yp1+4,0x00);
		write_buffer(buff);
		printf("yp1 %d\n",yp1);
	
}
void paddleAI()
{	
// player 2 AI on right part of the screen
		if (yb <= 5)
		{
			xp2 = 126;
			yp2 = 5;
		}
		else if (yb >= 59)
		{
			xp2 = 126;
			yp2 = 58;
		}
		else
		{
			xp2 = 126;
			yp2 = yb;
		}
	
	drawrect(buff,xp2-1,yp2-4,xp2,yp2+4,0x00);
	write_buffer(buff);
}

// Game counter 
void gamecounter()
{
	if(p1==0)
	drawstring(buff,48,7,"0");
	else if(p1==1)
	drawstring(buff,48,7,"1");
	else if(p1==2)
	drawstring(buff,48,7,"2");
	else if(p1==3)
	drawstring(buff,48,7,"3");
	else if(p1==4)
	drawstring(buff,48,7,"4");
	else if(p1==5)
	drawstring(buff,48,7,"5");
	
	if(p2==0)
	drawstring(buff,72,7,"0");
	else if(p2==1)
	drawstring(buff,72,7,"1");
	else if(p2==2)
	drawstring(buff,72,7,"2");
	else if(p2==3)
	drawstring(buff,72,7,"3");
	else if(p2==4)
	drawstring(buff,72,7,"4");
	else if(p2==5)
	drawstring(buff,72,7,"5");
	
}



//---------------------------------------GAME RESET--------------------------------------------
void resetgame()
{
	if (p1 == 5)
	{
		drawstring(buff,45,4,"P1 WINS");
		write_buffer(buff);
		_delay_ms(1000);
		for (int q=0;q<2;q++)
		{
		PORTD |= (1<<7);
		_delay_ms(5000);
		PORTD &= ~(1<<7);
		PORTB |= (1<<0);
		_delay_ms(5000);
		PORTB &= ~(1<<0);
		PORTB |= (1<<2);
		_delay_ms(5000);
		PORTB &= ~(1<<2);	
		}
		p1=0;
		p2=0;
		flag1=0;
	}
	else if (p2 == 5)
	{
		drawstring(buff,45,4,"P2 WINS");
		write_buffer(buff);
		_delay_ms(1000);
		for (int q=0;q<2;q++)
		{
			PORTD |= (1<<7);
			_delay_ms(5000);
			PORTD &= ~(1<<7);
			PORTB |= (1<<0);
			_delay_ms(5000);
			PORTB &= ~(1<<0);
			PORTB |= (1<<2);
			_delay_ms(5000);
			PORTB &= ~(1<<2);
		}
		p1=0;
		p2=0;
		flag1=0;
	}
}
//----------------------------THE BUZZER With interrupts-------------------------
void timer1_init()
{
	TCCR1A |= (1 << WGM12); //output compare CTC
	//TCCR1A |= (1 << 6); // toggle on o/p compare
	//TCCR1A &= ~(1 << 7); // as above
	TCCR1B |= (1 << CS10); // Pre scaler 1
	TIMSK1 |= (1 << 1); // output compare interrupt enabled
	TCNT1 = 0; // timer count initialization
	sei(); //global interrupt enabled
	DDRB |= (1<<1);//digital pin 9 is o/p
}

ISR(TIMER1_COMPA_vect)
{
	buz++;
	PORTB ^= (1<<1);
}

void buzzcond()
{
	if (b==1)
	{
		timer1_init();
		OCR1A = 20000;
		b=0;
	}
	else if (b==2)
	{
		timer1_init();
		OCR1A = 15000;
		b=0;
	}
	else if (b==3)
	{
		timer1_init();
		OCR1A = 7500;
		b=0;
	}
	if (buz>20)
	{
		buz=0;
		DDRB &= ~(1<<1);//digital pin 9 is o/p
		TIMSK1 &= ~(1 << 1);
	}
}


// ------------------------------------MAIN FUNCTION----------------------------------------

int main(void)
{
	uart_init();
			
	//setting up the gpio for backlight
	DDRD |= 0x80;
	PORTD &= ~0x80;
	PORTD |= 0x00;
	
	DDRB |= 0x05;
	PORTB &= ~0x05;
	PORTB |= 0x00;
	
	//lcd initialization
	lcd_init();
	lcd_command(CMD_DISPLAY_ON);
	lcd_set_brightness(0x18);
	write_buffer(buff);
	_delay_ms(10000);
	clear_buffer(buff);
	//int i=7; // Function selection

//-------------------ADC INITIALIZATION	-------------------
	adc_init(); //adc initialization - touchscreen
	//adc_init_accelo(); //adc initialization - accelerometer
	ADC = 0;		
//-------------------TIMER Initialization-----------------	
	//timer1_init();
		
	while (1)
	{
	/*	//Functions implementation
		if (i==1)
		{
		drawchar(buff,0,0,displayChar);
		write_buffer(buff);
		_delay_ms(5000);
		displayChar++;
		}
		else if (i==2)
		{
			setpixel(buff,50,20,0x00); //set pixel at particular location
			write_buffer(buff);
			_delay_ms(500);
			clearpixel(buff,50,20); //clear pixel at particular location
			write_buffer(buff);
			_delay_ms(500);
		}
		else if (i==3)
		{
			drawline(buff,0,0,60,50,0x00); // draw line
			write_buffer(buff);
		}
		else if (i==4)
		{
			drawrect(buff,50,20,60,25,BLACK); //rectangle
			write_buffer(buff);
		}
		else if (i==5)
		{
			fillrect(buff,50,5,127,60,BLACK); // filled rectangle
			write_buffer(buff);
		}
		else if (i==7)
		{
			fillcircle(buff,20,60,2,0x00); // filled circle
			write_buffer(buff);
		}
		else if (i==6)
		{
			drawcircle(buff,60,30,20,0x00); // draw circle
			write_buffer(buff);
		}
		else if (i==8)
		{
			drawcircle(buff,60,30,20,0x00); // draw string
			write_buffer(buff);
		}
	*/
	//---------Function demo end------------

	
	//---------- ---------------PONG GAME CODE------------------
	
	//The Touchscreen
	
	touch_screen_read();
	pixelcalc();
	//touchpixel();
	//-------------------
	
	
	//Accelerometer
	
	//accelo_read();
	//accelo_pixelcalc();
	//--------------------	
	
	
	//The Game outline, counter and background
	
	gamebg();
	gamecounter();
	//--------------------------------
		
	
	//Paddle and ball dynamics
	
	paddle();
	//paddleplayer();
	//paddleAI();
	
	ball();
	//--------------------------------
	buzzcond();
	
	
	printf("b %d   buz %d \n",b,buz);
	
	//GAME RESET
	resetgame();
	clear_buffer(buff); 
	}
	
}

