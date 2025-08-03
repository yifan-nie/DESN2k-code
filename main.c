// toggle project

#include "lpc24xx.h"

//#define FIO0DIR   *(volatile unsigned long *) 0x3FFFC000
//#define FIO3DIR   *(volatile unsigned long *) 0x3FFFC060
//#define FIO0PIN   *(volatile unsigned long *) 0x3FFFC014
//#define FIO3SET   *(volatile unsigned long *) 0x3FFFC078
//#define FIO3CLR   *(volatile unsigned long *) 0x3FFFC07C

//Paramter for: RTC
#define RTC_BASE_ADDR 0xE0024000
#define CCR   (*(volatile unsigned int *)(RTC_BASE_ADDR + 0x08))
#define SEC   (*(volatile unsigned int *)(RTC_BASE_ADDR + 0x20))
#define MIN   (*(volatile unsigned int *)(RTC_BASE_ADDR + 0x24))
#define HOUR  (*(volatile unsigned int *)(RTC_BASE_ADDR + 0x28))
#define DOM   (*(volatile unsigned int *)(RTC_BASE_ADDR + 0x2C))
#define MONTH (*(volatile unsigned int *)(RTC_BASE_ADDR + 0x34))
#define YEAR  (*(volatile unsigned int *)(RTC_BASE_ADDR + 0x38))
#define PREINT   (*((volatile unsigned int *)0xE0024080))
#define PREFRAC  (*((volatile unsigned int *)0xE0024084))
//---------------------------------------------------------------------------
//Paramters for: Ambient light sensor
#define AD0_BASE_ADDR		0xE0034000
#define AD0CR          (*(volatile unsigned long *)(AD0_BASE_ADDR + 0x00))
#define AD0DR1         (*(volatile unsigned long *)(AD0_BASE_ADDR + 0x14))
#define SCB_BASE_ADDR	0xE01FC000
#define PCONP          (*(volatile unsigned long *)(SCB_BASE_ADDR + 0x0C4))
//____________________________________________________
//Paramater for: power saving
#define RGB_MASK    (0x7Fu << 16)
#define LADDER_MASK (0xFFu << 1)
#define BKL_PIN     (1u << 18)
#define SPK_PIN     (1u << 25)

//Parameter for: Doorbell and smart plug
#define pulseW_F4 1432 //1432 us pulse width, half of period for F#4, 349Hz
#define pulseW_D4 6812 //6812 us pulse width, half of period for D4, 293Hz
#define F4_duration 500000
#define D4_duration 1000000
#define play_volume 0x1FF

#define L_B_MASK 0x400
#define R_B_MASK 0x800

//header for functions
void resetStart(void);
int lightsensor(void);
void udelay(unsigned int delay_in_us);
void setLEDColor(int is_red, int is_green, int is_blue);

//HERE IS SOME CHANGES
void RTC_Init(void);
void RTC_SetPrescaler(unsigned int pclk_hz);

//______________________________________________________________________________
int main(void) {

	int l_pushed = 0;
	int r_pushed = 0;
	int smartPlug_mask = 0x2;
	int rb_status = 0;
	int lb_status = 0;
	int time_played = 0;

	int blindState = -1;  // -1: unknown, 0: OFF, 1: ON
	int light  = 0;
	//---------------------------------------------------------------------------
	PINSEL1 = PINSEL1 | (1 << 21); //DAC setup
	FIO2DIR = FIO2DIR | smartPlug_mask;
	//enable LED ladder
	FIO0PIN = FIO0PIN | (1 << 22);
	RTC_Init();
	
	
	FIO3DIR |= (1 << 16) | (1 << 17) | (1 << 18) |
	           (1 << 19) | (1 << 20) | (1 << 21);

	while (1) {
		// read button input
		rb_status = (R_B_MASK & FIO0PIN) >> 11;
		lb_status = (L_B_MASK & FIO0PIN) >> 10;

		if (!l_pushed && lb_status) {
			l_pushed = 1;
			time_played = 0;
			while (time_played < F4_duration) {
				DACR = DACR | (play_volume << 6);
				udelay(pulseW_F4);
				time_played += pulseW_F4;
				DACR = DACR & ~(0x3FF << 6);
				udelay(pulseW_F4);
				time_played += pulseW_F4;
			}
			time_played = 0;
			while (time_played < D4_duration) {
				DACR = DACR | (play_volume << 6);
				udelay(pulseW_D4);
				time_played += pulseW_D4;
				DACR = DACR & ~(0x3FF << 6);
				udelay(pulseW_D4);
				time_played += pulseW_D4;
			}
		} else if (l_pushed && !rb_status) {
			l_pushed = 0;
		}

		if (!r_pushed && rb_status) {
			r_pushed = 1;
			FIO2PIN = FIO2PIN ^ smartPlug_mask;
		} else if (r_pushed && !rb_status) {
			r_pushed = 0;
		}

		//sample from light sensor
		light = lightsensor();
		if (light == 1 && blindState != 1) {
			setLEDColor(0, 0, 1);
			udelay(1000);
			setLEDColor(0, 1, 0);
			udelay(1000);
			setLEDColor(1, 0, 0);
			blindState = 1;
		} else if (light == 0 && blindState != 0) {
			setLEDColor(1, 0, 0);
			udelay(1000);
			setLEDColor(0, 1, 0);
			udelay(1000);
			setLEDColor(0, 0, 1);
			blindState = 0;
		}
	}
}

void udelay(unsigned int delay_in_us) {
	T0PR = 71;
	T0MR0 = delay_in_us;
	T0MCR |= 0x4;
	T0TCR |= 0x1;
	T0TCR &= ~(0x2);
	while ((T0TCR & 0x1) == 1) {
	}
	T0TCR |= 0x2;
	return;
}

void RTC_SetPrescaler(unsigned int pclk_hz) {
	unsigned int preint = (pclk_hz / 32768) - 1;
	unsigned int prefrac = pclk_hz - ((preint + 1) * 32768);
	PREINT = preint;
	PREFRAC = prefrac;
}

void RTC_Init(void) {
	PCONP |= (1 << 9);
	RTC_SetPrescaler(18000000);

	CCR = (1 << 1);
	CCR = 0x00;

	HOUR = 13;
	MIN  = 31;
	SEC  = 5;
	DOM  = 21;
	MONTH = 7;
	YEAR  = 2024;

	CCR |= (1 << 0);
}

int lightsensor(void) {
	int result;

	PCONP |= (1 << 12);

	PINSEL1 |= (1 << 16);
	PINSEL1 &= ~(1 << 17);

	AD0CR &= 0xF8DE0000;

	AD0CR = (1 << 21)
	      | (15 << 8)
	      | (1 << 1)
	      | (1 << 24);

	while ((AD0DR1 & 0x80000000) == 0);

	result = (AD0DR1 >> 6) & 0x3FFF;

	if (result > 153) {
		resetStart();
		return 1;
	} else {
		resetStart();
		return 0;
	}
}

void resetStart(void) {
	AD0CR &= ~(7 << 24);
	AD0CR |= (1 << 24);
}

void setLEDColor(int is_red, int is_green, int is_blue) {
	FIO3CLR = (1 << 16) | (1 << 17) | (1 << 18);
	FIO3CLR = (1 << 19) | (1 << 20) | (1 << 21);

	if (is_red) {
		FIO3SET = (1 << 16) | (1 << 19);
	}
	if (is_green) {
		FIO3SET = (1 << 17) | (1 << 20);
	}
	if (is_blue) {
		FIO3SET = (1 << 18) | (1 << 21);
	}
}
