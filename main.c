#include <reg52.h>
#include <intrins.h>

#define uchar unsigned char
#define uint  unsigned int

sbit DQ = P1^1;
sbit w1 = P2^4;
sbit w2 = P2^5;
sbit w3 = P2^6;
sbit w4 = P2^7;
sbit Buzzer  = P1^0;
sbit JdqLow  = P2^0;
sbit JdqHig  = P2^1;
sbit LedLow  = P2^2;
sbit LedHig  = P2^3;
sbit KeySet  = P3^2;
sbit KeyDown = P3^3;
sbit KeyUp   = P3^4;

uchar code Array1[] = { 0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,0x7f,0x6f,0x40 };
uchar code Array2[] = { 0xBf,0x86,0xdb,0xcf,0xe6,0xed,0xfd,0x87,0xff,0xef };

uchar Buff[4];
uchar ShowID = 1;

int AlarmLow = 150;`
int AlarmHig = 300;

void DelayMs(uint time)
{
	uint i, j;
	for(i = 0; i < time; i++)
		for(j = 0; j < 112; j++);
}

void Delay15us(void)
{
	_nop_(); _nop_(); _nop_(); _nop_(); _nop_(); _nop_(); _nop_(); _nop_();
	_nop_(); _nop_(); _nop_(); _nop_(); _nop_(); _nop_(); _nop_(); _nop_();
}

void DS18B20_ReSet(void)
{
	uchar i;
	DQ = 0;
	i = 240;
	while(--i);
	DQ = 1;
	i = 30;
	while(--i);
	while(~DQ);
	i = 4;
	while(--i);
}

void DS18B20_WriteByte(uchar dat)
{
	uchar j;
	uchar btmp;
	for(j = 0; j < 8; j++)
	{
		btmp = 0x01;
		btmp = btmp << j;
		btmp = btmp & dat;
		if(btmp > 0)
		{
			DQ = 0;
			Delay15us();
			DQ = 1;
			Delay15us(); Delay15us(); Delay15us(); Delay15us();
		}
		else
		{
			DQ = 0;
			Delay15us(); Delay15us(); Delay15us(); Delay15us();
			DQ = 1;
			Delay15us();
		}
	}
}

int DS18B20_ReadTemp(void)
{
	uchar j;
	int b, temp = 0;

	DS18B20_ReSet();
	DS18B20_WriteByte(0xcc);
	DS18B20_WriteByte(0x44);

	AT89S52_ReSet();
	DS18B20_WriteByte(0xcc);
	DS18B20_WriteByte(0xbe);

	for(j = 0; j < 16; j++)
	{
		DQ = 0;
		_nop_(); _nop_();
		DQ = 1;
		Delay15us();
		b = DQ;
		Delay15us(); Delay15us(); Delay15us();
		b = b << j;
		temp = temp | b;
	}

	temp = temp * 0.0625 * 10;
	return temp;
}

void TimerInit()
{
	TMOD = 0x01;
	TH0 = 248;
	TL0 = 48;
	ET0 = 1;
	EA = 1;
	TR0 = 1;
}

void ShowTemp(int dat)
{
	if(dat < 0)
	{
		Buff[0] = Array1[10];
		dat = 0 - dat;
	}
	else
	{
		Buff[0] = Array1[dat / 1000];
	}
	Buff[1] = Array1[dat % 1000 / 100];
	Buff[2] = Array2[dat % 100 / 10];
	Buff[3] = Array1[dat % 10];
}

void AlarmJudge(int dat)
{
	if(dat<AlarmLow)
	{
		LedLow=0;					
		LedHig=1;
		JdqLow=0;
		JdqHig=1;
		Buzzer=0;
	}
	else if(dat>AlarmHig)
	{
		LedLow=1;
		LedHig=0;
		JdqLow=1;
		JdqHig=0;
		Buzzer=0;
	}
	else
	{
		LedLow=1;
		LedHig=1;
		JdqLow=1;
		JdqHig=1;
		Buzzer=1;
	}
}

void KeyScanf()
{
	if(KeySet == 0)
	{
		LedLow = 0;
		LedHig = 1;
		Buzzer = 1;
		ShowTemp(AlarmLow);
		DelayMs(10);
		while(!KeySet);
		DelayMs(10);

		while(1)
		{
			if(KeyDown == 0)
			{
				if(AlarmLow > -550)
				{
					AlarmLow--;
					ShowTemp(AlarmLow);
					DelayMs(200);
				}
			}
			if(KeyUp == 0)
			{
				if(AlarmLow < 1250)
				{
					AlarmLow++;
					ShowTemp(AlarmLow);
					DelayMs(200);
				}
			}
			if(KeySet == 0)
			{
				break;
			}
		}

		LedLow = 1;
		LedHig = 0;
		ShowTemp(AlarmHig);
		DelayMs(10);
		while(!KeySet);
		DelayMs(10);

		while(1)
		{
			if(KeyDown == 0)
			{
				if(AlarmHig > -550)
				{
					AlarmHig--;
					ShowTemp(AlarmHig);
					DelayMs(200);
				}
			}
			if(KeyUp == 0)
			{
				if(AlarmHig < 1250)
				{
					AlarmHig++;
					ShowTemp(AlarmHig);
					DelayMs(200);
				}
			}
			if(KeySet == 0)
			{
				break;
			}
		}

		LedLow = 1;
		LedHig = 1;
		DelayMs(10);
		while(!KeySet);
		DelayMs(10);
	}
}

void main()
{
	int temp;
	uchar i;

	TimerInit();

	Buff[0] = Array1[0];
	Buff[1] = Array1[0];
	Buff[2] = Array1[0];
	Buff[3] = Array1[0];

	for(i = 0; i < 8; i++)
	{
		DS18B20_ReadTemp();
		DelayMs(120);
	}

	while(1)
	{
		EA = 0;
		temp = DS18B20_ReadTemp();
		EA = 1;

		ShowTemp(temp);
		AlarmJudge(temp);

		for(i = 0; i < 100; i++)
		{
			KeyScanf();
			DelayMs(10);
		}
	}
}

void Timer0(void) interrupt 1
{
	TH0 = 248;
	TL0 = 48;

	P0 = 0x00;
	w1 = 1;
	w2 = 1;
	w3 = 1;
	w4 = 1;

	if(ShowID == 1)
	{
		w1 = 0;
		P0 = Buff[0];
	}
	if(ShowID == 2)
	{
		w2 = 0;
		P0 = Buff[1];
	}
	if(ShowID == 3)
	{
		w3 = 0;
		P0 = Buff[2];
	}
	if(ShowID == 4)
	{
		w4 = 0;
		P0 = Buff[3];
	}

	ShowID++;
	if(ShowID == 5)
		ShowID = 1;
}
