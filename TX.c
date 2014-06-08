/*
 * RF_Tranceiver.c
 *
 * Created: 2012-08-10 15:24:35
 *  Author: Kalle
 *  Atmega88
 */ 

#include <avr/io.h>
#include <stdio.h>
#define F_CPU 1000000UL  // 8 MHz
#include <util/delay.h>
#include <avr/interrupt.h>

#include "nRF24L01.h"

#define dataLen 3  //data packet length
#define W 1
#define R 0

uint8_t *data;
uint8_t *arr;



void InitSPI(void)
{
	//Set SCK (PB5), MOSI (PB3) , CSN (SS & PB2) & C  as outport
	//OBS!!! has to be set before SPI-Enable below
	DDRB |= (1<<DDB5) | (1<<DDB3) | (1<<DDB2) |(1<<DDB1);
	
	/* Enable SPI, Master, set clock rate fck/16 .. clock rate not important */
	SPCR |= (1<<SPE)|(1<<MSTR);// |(1<<SPR0) |(1<<SPR1);
	
	SETBIT(PORTB, 2);	//CSN IR_High to start with, nothing to be sent to the RF yet!
	CLEARBIT(PORTB, 1);	//CE low to start with, nothing to send/recieve yet!
}

char WriteByteSPI(unsigned char cData)
{
	//Load byte to Data register
	SPDR = cData;
	
	/* Wait for transmission to complete */
	while(!(SPSR & (1<<SPIF)));
	
	// Return what's recieved from the nrf
	return SPDR;
}

uint8_t GetReg(uint8_t reg)
{
	//make sure last command was a while ago...
	_delay_us(10);
	CLEARBIT(PORTB, 2);	//CSN low - nRF starts to listen for command
	_delay_us(10);
	WriteByteSPI(R_REGISTER + reg);	//R_Register = set the NRF to reading mode, "reg" = this registry will be read back
	_delay_us(10);
	reg = WriteByteSPI(NOP);	//Send NOP (dummy byte) once to recieve back the first byte in the "reg" register
	_delay_us(10);
	SETBIT(PORTB, 2);	//CSN High - nRF goes back to doing nothing
	return reg;	// Return the read registry
}

uint8_t *WriteToNrf(uint8_t ReadWrite, uint8_t reg, uint8_t *val, uint8_t antVal)	//tar in "ReadWrite" (W el R), "reg" (ett register), "*val" (en array) & "antVal" (antal integer i variabeln)
{
	//cli();	//disable global interrupt
	
	if (ReadWrite == W)	//W=vill skriva till nrf-en (R=l�sa av den, R_REGISTER (0x00) ,s? skiter i en else funktion)
	{
		reg = W_REGISTER + reg;	//ex: reg = EN_AA: 0b0010 0000 + 0b0000 0001 = 0b0010 0001
	}
	
	//Static uint8_t f�r att det ska g? att returnera en array (l�gg m�rke till "*" uppe p? funktionen!!!)
	static uint8_t ret[32];	//antar att det l�ngsta man vill l�sa ut n�r man kallar p? "R" �r dataleng-l?ngt, dvs anv�nder man bara 1byte datalengd ? vill l�sa ut 5byte RF_Adress s? skriv 5 h�r ist!!!
	
	_delay_us(10);		//alla delay �r s? att nrfen ska hinna med! (microsekunder)
	CLEARBIT(PORTB, 2);	//CSN low = nrf-chippet b�rjar lyssna
	_delay_us(10);
	WriteByteSPI(reg);	//f�rsta SPI-kommandot efter CSN-l?g ber�ttar f�r nrf'en vilket av dess register som ska redigeras ex: 0b0010 0001 write to registry EN_AA
	_delay_us(10);
	
	int i;
	for(i=0; i<antVal; i++)
	{
		if (ReadWrite == R && reg != W_TX_PAYLOAD)
		{
			ret[i]=WriteByteSPI(NOP);	//Andra och resten av SPI kommandot s�ger ?t nrfen vilka v�rden som i det h�r fallet ska l�sas
			_delay_us(10);
		}
		else
		{
			WriteByteSPI(val[i]);	//Andra och resten av SPI kommandot s�ger ?t nrfen vilka v�rden som i det h�r fallet ska skrivas till
			_delay_us(10);
		}
	}
	SETBIT(PORTB, 2);	//CSN IR_High = nrf-chippet slutar lyssna
	
	return ret;	//returnerar en array
}

void nrf24L01_init(void)
{
	_delay_ms(100);	//allow radio to reach power down if shut down
	
	uint8_t val[5];	//en array av integers som skickar v�rden till WriteToNrf-funktionen

	//EN_AA - (auto-acknowledgements) - Transmittern f?r svar av recivern att packetet kommit fram, grymt!!! (beh�ver endast vara enablad p? Transmittern!)
	//Kr�ver att Transmittern �ven har satt SAMMA RF_Adress p? sin mottagarkanal nedan ex: RX_ADDR_P0 = TX_ADDR
	val[0]=0x01;	//ger f�rsta integern i arrayen "val" ett v�rde: 0x01=EN_AA p? pipe P0. 
	WriteToNrf(W, EN_AA, val, 1);	//W=ska skriva/�ndra n?t i nrfen, EN_AA=vilket register ska �ndras, val=en array med 1 till 32 v�rden  som ska skrivas till registret, 1=antal v�rden som ska l�sas ur "val" arrayen.
	
	//V�ljer vilken/vilka datapipes (0-5) som ska vara ig?ng.
	val[0]=0x01;
	WriteToNrf(W, EN_RXADDR, val, 1); //enable data pipe 0

	//RF_Adress width setup (hur m?nga byte ska RF_Adressen best? av? 1-5 bytes) (5bytes s�krare d? det finns st�rningar men l?ngsammare data�verf�ring) 5addr-32data-5addr-32data....
	val[0]=0x03;
	WriteToNrf(W, SETUP_AW, val, 1); //0b0000 00011 motsvarar 5byte RF_Adress
	
	//SETUP_RETR (the setup for "EN_AA")
	val[0]=0x2F;	//0b0010 00011 "2" sets it up to 750uS delay between every retry (at least 500us at 250kbps and if payload >5bytes in 1Mbps, and if payload >15byte in 2Mbps) "F" is number of retries (1-15, now 15)
	WriteToNrf(W, SETUP_RETR, val, 1);

	//RF channel setup - v�ljer frekvens 2,400-2,527GHz 1MHz/steg
	val[0]=0x01;
	WriteToNrf(W, RF_CH, val, 1); //RF channel registry 0b0000 0001 = 2,401GHz (samma p? TX ? RX)

	//RF setup	- v�ljer effekt och �verf�ringshastighet 
	val[0]=0x07;
	WriteToNrf(W, RF_SETUP, val, 1); //00000111 bit 3="0" ger l�gre �verf�ringshastighet 1Mbps=L�ngre r�ckvidd, bit 2-1 ger effektl�ge h�g (-0dB) ("11"=(-18dB) ger l�gre effekt =str�msn?lare men l�gre range)

	//RX RF_Adress setup 5 byte - v�ljer RF_Adressen p? Recivern (M?ste ges samma RF_Adress om Transmittern har EN_AA p?slaget!!!)
	int i;
	for(i=0; i<5; i++)	
	{
		val[i]=0x12;	//RF channel registry 0b10101011 x 5 - skriver samma RF_Adress 5ggr f�r att f? en l�tt och s�ker RF_Adress (samma p? transmitterns chip!!!)
	}
	WriteToNrf(W, RX_ADDR_P0, val, 5); //0b0010 1010 write registry - eftersom vi valde pipe 0 i "EN_RXADDR" ovan, ger vi RF_Adressen till denna pipe. (kan ge olika RF_Adresser till olika pipes och d�rmed lyssna p? olika transmittrar) 	
	
	//TX RF_Adress setup 5 byte -  v�ljer RF_Adressen p? Transmittern (kan kommenteras bort p? en "ren" Reciver)
	//int i; //?teranv�nder f�reg?ende i...
	for(i=0; i<5; i++)	
	{
		val[i]=0x12;	//RF channel registry 0b10111100 x 5 - skriver samma RF_Adress 5ggr f�r att f? en l�tt och s�ker RF_Adress (samma p? Reciverns chip och p? RX-RF_Adressen ovan om EN_AA enablats!!!)
	}
	WriteToNrf(W, TX_ADDR, val, 5); 

	// payload width setup - Hur m?nga byte ska skickas per s�ndning? 1-32byte 
	val[0]=dataLen;		//"0b0000 0001"=1 byte per 5byte RF_Adress  (kan v�lja upp till "0b00100000"=32byte/5byte RF_Adress) (definierat h�gst uppe i global variabel!)
	WriteToNrf(W, RX_PW_P0, val, 1);
	
	//CONFIG reg setup - Nu �r allt inst�llt, boota upp nrf'en och g�r den antingen Transmitter lr Reciver
	val[0]=0x1E;  //0b0000 1110 config registry	bit "1":1=power up,  bit "0":0=transmitter (bit "0":1=Reciver) (bit "4":1=>mask_Max_RT,dvs IRQ-vektorn reagerar inte om s�ndningen misslyckades. 
	WriteToNrf(W, CONFIG, val, 1);

//device need 1.5ms to reach standby mode
	_delay_ms(100);	

	//sei();	
}

void transmit_payload(uint8_t * W_buff)
{
	WriteToNrf(R, FLUSH_TX, W_buff, 0); //skickar 0xE1 som flushar registret f�r att gammal data inte ska ligga ? v�nta p? att bli skickad n�r man vill skicka ny data! R st?r f�r att W_REGISTER inte ska l�ggas till. skickar inget kommando efterr?t eftersom det inte beh�vs! W_buff[]st?r bara d�r f�r att en array m?ste finnas d�r...
	
	WriteToNrf(R, W_TX_PAYLOAD, W_buff, dataLen);	//skickar datan i W_buff till nrf-en (obs g?r ej att l�sa w_tx_payload-registret!!!)
	
	//sei();	//enable global interrupt- redan p?!
	//USART_Transmit(GetReg(STATUS));

	_delay_ms(10);		//beh�ver det verkligen vara ms ? inte us??? JAAAAAA! annars funkar det inte!!!
	SETBIT(PORTB, 1);	//CE h�g=s�nd data	INT0 interruptet k�rs n�r s�ndningen lyckats och om EN_AA �r p?, ocks? svaret fr?n recivern �r mottagen
	_delay_us(20);		//minst 10us!
	CLEARBIT(PORTB, 1);	//CE l?g
	_delay_ms(10);		//beh�ver det verkligen vara ms ? inte us??? JAAAAAA! annars funkar det inte!!!

	//cli();	//Disable global interrupt... ajabaja, d? st�ngs USART_RX-lyssningen av!

}

void receive_payload(void)
{
	//sei();		//Enable global interrupt
	
	SETBIT(PORTB, 1);	//CE IR_High = "Lyssnar"
	_delay_ms(1000);	//lyssnar i 1s och om mottaget g?r int0-interruptvektor ig?ng
	CLEARBIT(PORTB, 1); //ce l?g igen -sluta lyssna
	
	//cli();	//Disable global interrupt
}

//small led indicator
void blinky(){
	int i;
	for (i=0; i<5; i++){
		SETBIT(PORTB,0);		
		_delay_ms(50);
		CLEARBIT(PORTB,0);
		_delay_ms(50);
	}
}

//smaller led indicator
void blinky2(){
	int i;
		SETBIT(PORTB,0);		
		_delay_ms(50);
		CLEARBIT(PORTB,0);
		_delay_ms(50);
}

//test SPI
void test(){
	
	uint8_t val[5];

	reset();
	_delay_ms(500);
	
	if (GetReg(STATUS)==0x0E){
		blinky();
	}else
	test();
	
	_delay_ms(500);

}

void reset(void)
{
	_delay_us(10);
	CLEARBIT(PORTB, 2);	//CSN low
	_delay_us(10);
	WriteByteSPI(W_REGISTER + STATUS);	//
	_delay_us(10);
	WriteByteSPI(0x70);	//radedrar alla irq i statusregistret (f�r att kunna lyssna igen)
	_delay_us(10);
	SETBIT(PORTB, 2);	//CSN IR_High
}

void ioinit(void)
{
	DDRB |= (1<<DDB0); //led
}

// transmit packet every 1 sec, and blink led shortly
int transmit(){
	while(1)
	{
		reset();
		_delay_ms(100);
		uint8_t W_buffer[3];
		int i;
		for (i=0; i<3; i++){
			W_buffer[i] = 0x93;
		}
		transmit_payload(W_buffer);
		_delay_ms(100);
		
		//check for successful writing
		if (GetReg(SETUP_RETR) == 0x2F)
		{
			blinky2();
		}
		_delay_ms(1000);
	}
}

int main(void)
{
	_delay_ms(5000);
	InitSPI();
	ioinit();
	//INT0_interrupt_init();
	nrf24L01_init();

	SETBIT(PORTB,0);
	_delay_ms(500);
	CLEARBIT(PORTB,0);
	_delay_ms(200);

	//test SPI
	test();
	
	_delay_ms(200);
	
	//transmit loop
	transmit();
	
	return 0;
}