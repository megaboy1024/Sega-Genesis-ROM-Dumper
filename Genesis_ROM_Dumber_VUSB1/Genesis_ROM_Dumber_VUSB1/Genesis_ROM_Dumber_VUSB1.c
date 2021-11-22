/*
 * Genesis_ROM_Dumber_VUSB1.c
 *
 * Created: 8/31/2013 11:15:42 PM
 *  Author: megaboy
 */ 


#include <avr/io.h>
#include <util/delay.h>
//usbdrv necessary headers
#include "usbdrv/usbdrv.h"
#include <avr/interrupt.h>
#include <avr/wdt.h>

#define	LED_PORT	PORTB
#define	LED_DDR		DDRB
#define	LED_1		0
#define LED_1_ON()	LED_PORT &=~(1<<LED_1)
#define LED_1_OFF()	LED_PORT|=(1<<LED_1)

#define DELAY_MS(n)	_delay_ms(n)
#define DELAY_US(n)	_delay_us(n)

// ROM IO definitions
#define Serial_ADDRESS_PORT	PORTD
#define Serial_ADDRESS_DDR	DDRD
#define SER_ADDRESS_PIN		4//were 7 replaced with usbd_minus for using OC1A
#define SCK_ADDRESS_PIN		6
#define RCK_ADDRESS_PIN		5
#define CART_IN			3
#define CART_IN_DDR			DDRD
#define CART_IN_PIN			PIND

#define CTRL_PORT	PORTB
#define CTRL_DDR	DDRB

#define OE_PIN		3//it were on 0 before adding USB port
#define CE_PIN		1

#define SER_Low()	Serial_ADDRESS_PORT&=~_BV(SER_ADDRESS_PIN)
#define SER_High()	Serial_ADDRESS_PORT|=_BV(SER_ADDRESS_PIN)

#define SCK_Low()	Serial_ADDRESS_PORT&=~_BV(SCK_ADDRESS_PIN)
#define SCK_High()	Serial_ADDRESS_PORT|=_BV(SCK_ADDRESS_PIN)

#define RCK_Low()	Serial_ADDRESS_PORT&=~_BV(RCK_ADDRESS_PIN)
#define RCK_High()	Serial_ADDRESS_PORT|=_BV(RCK_ADDRESS_PIN)

#define OE_Low()	CTRL_PORT&=~_BV(OE_PIN)
#define OE_High()	CTRL_PORT|=_BV(OE_PIN)

#define CE_Low()	CTRL_PORT&=~_BV(CE_PIN)
#define CE_High()	CTRL_PORT|=_BV(CE_PIN)

#define DATA_LOW_PORT	PORTA
#define DATA_LOW_DDR	DDRA
#define DATA_HIGH_PORT	PORTC
#define DATA_HIGH_DDR	DDRC
volatile uint8_t cart_flag=0;
static uint32_t	ROM_Size=0;
static uint32_t Block_Offset=0;
static uint32_t	long_offset=0;
static uint8_t	block[512];
uint8_t	Console_NAME[17]="";//H100
uint8_t	Copyright_Notice[17]="";//H110
uint8_t	Domestic_Game_Name[49]="";//H120
uint8_t	Overseas_Game_Name[49]="";//H150
uint8_t	Product_Type[3]="";//H180
uint8_t	Product_code[13]="";//H182
uint8_t	Check_Sum[2];//H18E
uint8_t	Supported_IO[17]="";//H190
uint8_t	ROM_Start_Address[4]="";//H1A0
uint8_t	ROM_End_Address[4]="";//H1A4
uint8_t	Backup_RAM_Start_Address[4]="";//H1A8
uint8_t	Backup_RAM_END_Address[4]="";//H1AC
uint8_t	MODEM_Support[10]="";//H1BC
uint8_t	ROM_Info[29]="";//H1C8
uint8_t	Country_Region[4]="";//H1F0  TODO::solve The 3rd character problem in Information


//usb vendor-specific messages
#define USB_LED_OFF	0
#define USB_LED_ON	1
#define USB_DATA_OUT 2
#define USB_DATA_WRITE 3
#define USB_DATA_IN 4
#define USB_DATA_LONGOUT 5
//TODO:: define messages for Detecting ROM info and transferring it
#define USB_ROM_SIZE    6//Like USB_DATA_OUT
#define USB_BLOCK_NUM   7
#define USB_Console_Name 8
#define USB_DUMP_BLOCK	9
#define USB_READ_ADDRESS 10

static int dataSent;

static uchar replyBuf[16] = "Hello, USB!";//buffer for Data out to host

static uchar dataReceived = 0, dataLength = 0; // for USB_DATA_IN

//IO init function
void	IO_init(void)
{
	LED_DDR|=(1<<LED_1);
	LED_PORT|=(1<<LED_1);
	//ROM IO init
	Serial_ADDRESS_DDR|=(1<<SER_ADDRESS_PIN)|(1<<SCK_ADDRESS_PIN)|(1<<RCK_ADDRESS_PIN);//set SER,SCK,RCK Pins as Output
	Serial_ADDRESS_DDR&=~(1<<CART_IN);//set /CART_IN Pin as Input
	Serial_ADDRESS_PORT&=~(_BV(SER_ADDRESS_PIN)|_BV(SCK_ADDRESS_PIN)|_BV(RCK_ADDRESS_PIN));
	Serial_ADDRESS_PORT|=_BV(CART_IN);
	
	CTRL_DDR|=_BV(OE_PIN)|_BV(CE_PIN);
	CTRL_PORT|=_BV(OE_PIN)|_BV(CE_PIN);
	
	DATA_LOW_DDR=0x00;
	DATA_LOW_PORT=0xff;
	DATA_HIGH_DDR=0x00;
	DATA_HIGH_PORT=0xff;	
}


void	SCK_Pulse()
{
	SCK_Low();
	_delay_us(1);
	SCK_High();
}

void	RCK_Pulse()
{
	RCK_Low();
	_delay_us(1);
	RCK_High();
}


uint16_t	Read_ADDRESS(long	Address)
{
	uint16_t	data=0;
	CE_Low();
	Address=(Address<<1)&0xfffffffe;
	for(int i=0;i<=23;i++)
	{
		if((Address>>i)&0x01)
		{
			SER_High();
		}
		else
		{
			SER_Low();
		}
		SCK_Pulse();
	}
	RCK_Pulse();
	OE_Low();
	//_delay_ms(2);
	_delay_us(10);
	data=PINC;
	data=data<<8;
	data|=PINA;
	OE_High();
	//CE_High();
	return data;
}

uint32_t	Get_ROM_Size()
{
	uint32_t	End_Address=0;
	End_Address=Read_ADDRESS(0x1A4/2);
	End_Address=End_Address<<16;
	End_Address|=Read_ADDRESS(0x1A6/2);
	return End_Address+1;
}

uint32_t	Get_Backup_RAM_Size()
{
	uint32_t	End_Address=0;
	End_Address=Read_ADDRESS(0x1AC/2);
	End_Address=End_Address<<16;
	End_Address|=Read_ADDRESS(0x1AD/2);
	return End_Address+1;
}

void	dump_block(uint8_t *block1,uint32_t ofs,uint32_t cnt)
{
	uint32_t sub_address=0;
	uint32_t count=0;
	uint16_t Data=0;
	
	for(count=ofs/2;count<((ofs+cnt)/2);count++)//TODO::Recalculate Length for 8 16 Bit Different
	{
		//if(count%2==0)
		Data=Read_ADDRESS(count);
		block1[sub_address]=(Data&0xff00)>>8;
		block1[sub_address+1]=(Data&0xff);
		sub_address+=2;
	}
}

//static void put_dump_ROM (char *buff, DWORD ofs, int cnt)
//{
	//BYTE n;
//
//
	//xitoa(ofs, 16, -8); xputc(' ');
	//for(n = 0; n < cnt; n++) {
		//xputc(' ');	xitoa(buff[n+ofs], 16, -2);
	//}
	//xputs(PSTR("  "));
	//for(n = 0; n < cnt; n++)
	//xputc(((buff[n+ofs] < 0x20)||(buff[n+ofs] >= 0x7F)) ? '.' : buff[n+ofs]);
	//xputc('\n');
//}

void ROM_Information()
{
	
	
	
	dump_block(Console_NAME,0x100,0x10);
	//xprintf(PSTR("\nConsole Name : \t\t\t%s\n"),Console_NAME);

	dump_block(Copyright_Notice,0x110,0x10);
	//xprintf(PSTR("Copyright Info : \t\t%s\n"),Copyright_Notice);
	
	dump_block(Domestic_Game_Name,0x120,48);
	//xprintf(PSTR("\nDomestic Game Name : \t\t%s\n"),Domestic_Game_Name);
	
	dump_block(Overseas_Game_Name,0x150,48);
	//xprintf(PSTR("Overseas Game Name : \t\t%s\n"),Overseas_Game_Name);
	
	dump_block(Product_Type,0x180,2);
	//xprintf(PSTR("\nProduct Type : \t\t\t%s\n"),Product_Type);
	
	dump_block(Product_code,0x182,11);
	//xprintf(PSTR("Product code : \t\t\t%s\n"),Product_code);
	
	dump_block(Check_Sum,0x18E,2);
	//xprintf(PSTR("\nCheck Sum : \t\t\t0x"));
	//xitoa(Check_Sum[0],16,-2);xitoa(Check_Sum[1],16,-2);xputc('\n');
	
	dump_block(Supported_IO,0x190,16);
	//xprintf(PSTR("\nSupported IO : \t\t\t%s\n"),Supported_IO);
	
	dump_block(ROM_Start_Address,0x1A0,4);
	//xprintf(PSTR("\nROM Start Address : \t\t0x"));
	//xitoa(ROM_Start_Address[0],16,-2);xitoa(ROM_Start_Address[1],16,-2);
	//xitoa(ROM_Start_Address[2],16,-2);xitoa(ROM_Start_Address[3],16,-2);xputc('\n');
	
	dump_block(ROM_End_Address,0x1A4,4);
	//xprintf(PSTR("ROM End   Address : \t\t0x"));
	//xitoa(ROM_End_Address[0],16,-2);xitoa(ROM_End_Address[1],16,-2);
	//xitoa(ROM_End_Address[2],16,-2);xitoa(ROM_End_Address[3],16,-2);xputc('\n');
	
	dump_block(Backup_RAM_Start_Address,0x1A8,4);
	//xprintf(PSTR("\nBackup RAM Start Address : \t0x"));
	//xitoa(Backup_RAM_Start_Address[0],16,-2);xitoa(Backup_RAM_Start_Address[1],16,-2);
	//xitoa(Backup_RAM_Start_Address[2],16,-2);xitoa(Backup_RAM_Start_Address[3],16,-2);xputc('\n');
	
	dump_block(Backup_RAM_END_Address,0x1AC,4);
	//xprintf(PSTR("Backup RAM End   Address : \t0x"));
	//xitoa(Backup_RAM_END_Address[0],16,-2);xitoa(Backup_RAM_END_Address[1],16,-2);
	//xitoa(Backup_RAM_END_Address[2],16,-2);xitoa(Backup_RAM_END_Address[3],16,-2);xputc('\n');
	
	
	
	dump_block(MODEM_Support,0x1BC,9);
	//xprintf(PSTR("\nMODEM Support : \t\t%s\n"),MODEM_Support);
	
	dump_block(ROM_Info,0x1C8,28);
	//xprintf(PSTR("ROM Info : \t\t%s\n"),ROM_Info);
	
	dump_block(Country_Region,0x1F0,3);
	//xprintf(PSTR("Country Region : \t\t%s\n"),Country_Region);
	
}

// this gets called when custom control message is received
// Change the return type from uchar to usbMsgLen_t when enabling long transfers in usbconfig
USB_PUBLIC	usbMsgLen_t	usbFunctionSetup(uchar data[8])
{
	usbRequest_t	*rq=(void *)data;// cast data to correct type
	
	switch (rq->bRequest)//custom command is in the bRequest field
	{
	case USB_LED_ON:
		LED_1_ON(); //turn LED1 on
		return	0;
		
	case USB_LED_OFF:
		LED_1_OFF(); //turn LED1 off
		return	0;
		
	case USB_DATA_OUT: // send data to PC
		usbMsgPtr = replyBuf;
		return sizeof(replyBuf);
		
	case USB_DATA_WRITE: // modify reply buffer
		replyBuf[7] = rq->wValue.bytes[0];
		replyBuf[8] = rq->wValue.bytes[1];
		replyBuf[9] = rq->wIndex.bytes[0];
		replyBuf[10] = rq->wIndex.bytes[1];
		return	0;
		
	case USB_DATA_IN: // receive data from PC
		dataLength = (uchar)rq->wLength.word;
		dataReceived = 0;
	
		if(dataLength > sizeof(replyBuf)) // limit to buffer size
		dataLength = sizeof(replyBuf);
	
		return USB_NO_MSG; // usbFunctionWrite will be called now
		
	case USB_DATA_LONGOUT: // send data to PC
		dataSent = 0;
		return USB_NO_MSG;
	case USB_ROM_SIZE:
		ROM_Size=Get_ROM_Size();
		replyBuf[0]=ROM_Size>>24;
		replyBuf[1]=ROM_Size>>16;
		replyBuf[2]=ROM_Size>>8;
		replyBuf[3]=ROM_Size&0xff;
		usbMsgPtr=replyBuf;
		return	4;//number of bytes to sent
	case USB_Console_Name:
		ROM_Information();
		usbMsgPtr=Console_NAME;
		return sizeof(Console_NAME);
	case USB_DUMP_BLOCK: // send data to PC
		//dataSent = 0;
		Block_Offset=((rq->wValue.word)|(rq->wIndex.word));
		dump_block(block,Block_Offset*512,512);
		usbMsgPtr=block;
		return 512;
		
	}
	
	return	0;//should not get here
}

//usb write function to receive data sent from host
USB_PUBLIC uchar usbFunctionWrite(uchar *data, uchar len) {
	uchar i;
	
	for(i = 0; dataReceived < dataLength && i < len; i++, dataReceived++)
	replyBuf[dataReceived] = data[i];
	
	return (dataReceived == dataLength); // 1 if we received it all, 0 if not
}

//function usbFunctionRead for transferring long data to host
USB_PUBLIC uchar usbFunctionRead(uchar *data, uchar len) {
	uchar i;

	for(i = 0; dataSent < 512 && i < len; i++, dataSent++)
	//data[i] = '0'+i;
	data[i] = (uchar)block[i+long_offset];
	//long_offset++;
	
	
	// terminate the string if it's the last byte sent
	//if(i && dataSent == 512)
	//data[i-1] = 0; // NULL
	
	return i; // equals the amount of bytes written
}

int main(void)
{
	uchar	i;
	
	wdt_enable(WDTO_1S);//enable 1s watchdog timer
	IO_init();
	
 	//char	block[512];
 	//uint32_t i=0;
 	//uint32_t offset=0;
	MCUCR|=_BV(ISC10);//Enable INT1 on any Logical Change
	GICR|=_BV(INT1);
	
	
		 
	usbInit();
	
		if(!(CART_IN_PIN&_BV(CART_IN)))
	{
		cart_flag=1;
		//xprintf(PSTR("Cartridge Detected..\n"));
		//_delay_ms(100);
		//xprintf(PSTR("Reading Cartridge Info...\n"));
		//_delay_ms(1000);
		ROM_Information();
	}
	
 	//uint32_t j=0,k=0;	
	 
	usbDeviceDisconnect();//enforce re-enumeration
	for (i=0;i<250; i++)//wait 500 ms
	{
		wdt_reset();//keep the watchdog happy
		_delay_ms(2);
	}
	
	usbDeviceConnect();
	
	sei();//Enable interrupts after re-enumeration
	
    while(1)
    {
        //TODO:: Please write your application code 
		//LED_1_ON();
		//DELAY_MS(100);
		//LED_1_OFF();
		//DELAY_MS(100);
		
		wdt_reset();//keep the watchdog happy
		usbPoll();
    }
}


ISR(INT1_vect)
{
	if((CART_IN_PIN&_BV(CART_IN))&&(cart_flag==1))
	{
		cart_flag=0;
		//xprintf(PSTR("Cartridge Removed!\n"));
		//_delay_ms(1000);
	}
	else if((!(CART_IN_PIN&_BV(CART_IN)))&&(cart_flag==0))
	{
		cart_flag=1;
		//xprintf(PSTR("Cartridge Inserted!\n"));
		//_delay_ms(100);
		//xprintf(PSTR("Reading Cartridge Info...\n"));
		//_delay_ms(1000);
		//ROM_Information();
		//_delay_ms(100);
	}
}