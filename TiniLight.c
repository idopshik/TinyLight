/*
 * TiniLight.c
 *
 * Created: 29.06.2016 18:32:30
 *  Author: idopshik
 */
// ver 2.0 от 15.01.17
// ver 3.0    12.02.17 . includes charge level indication

#include <avr/io.h>
#define F_CPU 4800000UL  //
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#define button_is_broken 32000
#define LongPress		 14500
#define ShortPress		 600

#define VoltageLOW		 124
#define VoltageMiddle	 126
#define VoltageFULL		 129


uint8_t MODE;
uint16_t DebounceVar;
uint8_t Last_button_state ;				                 // 0 -   здесь "неактивна", не нажата.
uint8_t LongDone ;
uint16_t DebounceTurnOn ;
uint8_t ADC_8bit_result ;

void adcInit(void)
{
																// SC - start conversion IE - interrupt enable  ADATE - автоход. Здесь он выключен.

	ADCSRA&= ~(1<<ADATE);						//убираем автоход
	ADCSRA|= (1<<ADEN)|(1<ADSC)|(1<<ADPS0)|(1<<ADPS1)|(1<<ADPS2);

	ADMUX|= (1<<REFS0);						// internal voltage ref.
	ADMUX|= (1<<ADLAR);						// left adjust
	ADMUX |=(1<<MUX0)|(1<<MUX1);					// pb3 / ADC3

	DIDR0 = (1<<ADC0D)|(1<<ADC1D)|(1<<ADC2D);
	ADCSRA|=(1<ADSC);						//ADSC  начало преобразования
																// adlar-1 - читаем только ADCH

}

void MeasureVoltage(void)
{
	DDRB |=(1<<4);							//gait of MOP transistor
	PORTB |=(1<<4);							//включим транзистор.
	_delay_us(100);							// пусть установится ток в делителе!

	adcInit();

	ADCSRA |= (1<<ADSC);						// Start conversion by setting ADSC in ADCSRA Register
	while (ADCSRA & (1<<ADSC));
	ADCSRA |= (1<<ADSC);						//первый блин комом, стартуем ещё раз
	while (ADCSRA & (1<<ADSC));
	ADC_8bit_result=ADCH;						//порту C присвоить 8м бит регистра данных ADCH

																// Досвидос
	ADMUX=0;
	ADCSRA=0;
	PORTB &= ~(1<<4);						//выкл транзистор, экономим ток.
}


void ShowLevel(uint8_t Level)
{
	uint8_t i;

	TCCR0A &= ~(1<<COM0A1);						//отключили
	_delay_ms(50);
	OCR0A = 80;							// Ярче

	if(Level>4){
	Level-=5;
	TCCR0A |=(1<<COM0A1);						//Пин подключен к таймеру
	_delay_ms(40);							// Горит длительно

	TCCR0A &= ~(1<<COM0A1);						//отключили
	_delay_ms(20);							// пауза
	}
	for(i=Level;i>0;i--)
	{

	TCCR0A |=(1<<COM0A1);						//Пин подключен к таймеру
	_delay_ms(10);							// Горит

	TCCR0A &= ~(1<<COM0A1);						//отключил
	_delay_ms(10);							// пауза
	}

	TCCR0A |=(1<<COM0A1);						//Пин подключен к таймеру

}


uint8_t Determine_level(void)
{
	uint8_t var;
	var = 10;
	uint8_t i;
for (i = ADC_8bit_result; i<VoltageFULL; i++)
{
	var--;
	if(!var)return 0;						// Предохранение от перехода через нуль при большой разнице
}
return var;
}


void SetAll(void)
{
									//Set Timer 0
	TCCR0A |=(1<<COM0A1);						// Com0A -set at Top,clear at compare
	TCCR0A |= (1<<WGM00)|(1<<WGM01);				// FAST PWM
	TCCR0B |= (1<<CS00);						// no prescaling
	DDRB |= (1<<0);
	DDRB &= ~(1<<1);						//Подтяг не обязательно, там внешний есть 10к.
									// Все коэффициенты в нуль.

	Last_button_state = 0;						//Надо для логики.
	DebounceTurnOn = 0;
	LongDone = 0;

	cli();
}

ISR(INT0_vect)								// Внешнее hardware-прерывание (нажатием кнопки).
{
	MCUCR &= ~((1<<SE)|(1<<SM1)|(1<<ISC01));			// turn off Sleep Mode and also INT0_vect
	GIMSK &=~(1<<INT0);						// for it dosn't disturb normal run of app.
	cli();								// Запрещаем прерываения, даём нормально работать.
	SetAll();
}

//----------------------------------------------------------------------//
void Sleep(void)
{
	MODE=0;
	TCCR0A = 0;							// Таймер выкл.
	TCCR0B = 0;
	OCR0A = 0;
	DDRB = 0;							// Порт выкл.
	PORTB =0;
	MCUCR |= (1<<ISC01);						// Falling Edge int0;
	GIMSK |=(1<<INT0);
	sei();
	MCUCR |= (1<<SE)|(1<<SM1);					// Sleep Enable + Power Down
}

int main(void)
{
SetAll();
 while(1)
 {
//------------------------------------------------------------------//	//если нажали на кнопку SW1
	if(~PINB&(1<<1))
	 {
		if(!Last_button_state)					//начинаем сначала
		{
			Last_button_state = 1;
			DebounceVar =0;
			LongDone = 0;					//Снова разрешаем длинное нажатие
		}
		if (!LongDone)DebounceVar++;				// Инкремент счётчика итераций
		if ((DebounceVar>LongPress)&&(!LongDone))		//Не было длинного действия
		{
			LongDone = 1;
			if (MODE == 1)
			{
				MeasureVoltage();
				ShowLevel(Determine_level());
				Sleep();
				continue;
			}
																		//Лампа работала - значит выключаем
			else													    //Лампа была выключена
			{
				OCR0A = 20;
				TCCR0A |=(1<<COM0A1);				//Пин подключен к таймеру
				MODE = 1;												// Просыпаемся
			}
		}
	  }

//------------------------------------------------------------------------------////сейчас не нажата)

	  if(PINB&(1<<1))
	  {
			if ((Last_button_state == 1)&&(MODE == 1))		//Кнопка до этого была нажата
			{
				if (LongDone) DebounceVar = 0;			// Отсекаем ложное короткое после длинного.
				if (DebounceVar>ShortPress)	OCR0A += 40;	//Было короткое нажатие Меняем яркость
			}
			Last_button_state = 0;					//зафиксировали текущее состояние кнопки
			DebounceTurnOn++;											// Стартует от одного. Надо чтобы через нуль переполнился

			if ((DebounceTurnOn>65000)&&(MODE==0))
			{
				DebounceTurnOn=0;										//Эта строчка - цена длительного дебага. Почему то без неё она засыпает
																		// и просыпаясь выполняет прерывание дважды. То есть первое прерываение не очищает толи флаг какой-то, толи ещё что. Она должна
																		// после того как проснётся запретить прерывания и всё. А она их только на второй раз запщещает (прерывание срабатывет два раза, второй лишний).

				Sleep();												// Недовключил, и он заснул.
				continue;
			}
	  }
  }
}
