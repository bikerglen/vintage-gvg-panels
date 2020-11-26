//=============================================================================================
// INCLUDES
//

#include "SI_EFM8UB2_Register_Enums.h"
#include "efm8_usb.h"
#include "descriptors.h"
#include "idle.h"
#include "InitDevice.h"
#include "fonts.h"


//=============================================================================================
// DEFINES
//

#define SYSCLK      48000000      // SYSCLK frequency in Hz

#define DATA        P1

#define DIR_EFM2XPT 1
#define DIR_XPT2EFM 0

#define RIGHT_SIDE  0
#define LEFT_SIDE   1

#define PIC1        0x00
#define PIC2        0x04

#define LCD_CMD     0x01
#define LCD_DATA    0x05
#define LCD_SELECT  0x06

#define DISPLAY0    0x00
#define DISPLAY1    0x04
#define DISPLAY2    0x08


//=============================================================================================
// SI_SBITS
//

SI_SBIT(D7,        SFR_P1, 7);
SI_SBIT(D6,        SFR_P1, 6);
SI_SBIT(D5,        SFR_P1, 5);
SI_SBIT(D4,        SFR_P1, 4);
SI_SBIT(D3,        SFR_P1, 3);
SI_SBIT(D2,        SFR_P1, 2);
SI_SBIT(D1,        SFR_P1, 1);
SI_SBIT(D0,        SFR_P1, 0);

SI_SBIT(RST_n,     SFR_P0, 7);
SI_SBIT(A0,        SFR_P0, 6);
SI_SBIT(A1,        SFR_P0, 5);
SI_SBIT(A2,        SFR_P0, 4);
SI_SBIT(WR_n,      SFR_P0, 3);
SI_SBIT(RD_n,      SFR_P0, 2);
SI_SBIT(CSL_n,     SFR_P0, 1);
SI_SBIT(CSR_n,     SFR_P0, 0);

SI_SBIT(DIR,       SFR_P2, 1);

SI_SBIT(LED,       SFR_P3, 5);


//=============================================================================================
// TYPEDEFS
//

typedef struct {
	uint8_t rxpkt_state;
	uint8_t rxpkt_cksum;
	uint8_t rxpkt_length;
	uint8_t rxpkt_count;
	uint8_t rxpkt_buffer[32];
} rxpkt_t;


//=============================================================================================
// PROTOTYPES
//

void Timer2_Init (int counts);

void SendPicPacket (uint8_t side, uint8_t pic, uint8_t length, uint8_t *dpkt);
uint8_t ReceivePicPacket (rxpkt_t *rx, uint8_t d);
void WaitPicPacket (void);
void WritePic (uint8_t side, uint8_t pic, uint8_t d);
uint8_t ReadPic (uint8_t side, uint8_t pic);

void SetTallyLight (uint8_t side, uint8_t pic, uint8_t tally);
void ClearTallyLight (uint8_t side, uint8_t pic, uint8_t tally);
void ClearSetTallyLights (uint8_t side, uint8_t pic, uint8_t clear, uint8_t set);

void WriteFpgaBegin (void);
void WriteFpga (uint8_t side, uint8_t a, uint8_t d);
void SetLcdBacklights (uint8_t side, uint8_t display, uint8_t redtop, uint8_t redbot, uint8_t grntop, uint8_t grnbot);
void ResetLcd (uint8_t side, uint8_t display);
void InitLcd (uint8_t side, uint8_t display);
void DisplayImage (uint8_t side, uint8_t display, uint8_t window, uint8_t *image);
void DisplayImageRow (uint8_t side, uint8_t display, uint8_t window, uint8_t row, uint8_t *image);
void DisplayText5x7 (uint8_t side, uint8_t display, uint8_t window, uint8_t row, char *text);
void DisplayText8x8 (uint8_t side, uint8_t display, uint8_t window, uint8_t row, char *text);
void DisplayText10x14 (uint8_t side, uint8_t display, uint8_t window, uint8_t row, char *text);


//=============================================================================================
// GLOBALS
//

volatile uint8_t flag100 = 0;

uint8_t xdata txpkt[48];

rxpkt_t xdata rxpktR1;

volatile uint8_t xdata keyReportNeeded = false;
volatile uint8_t xdata keyReportData[8];
volatile uint8_t xdata tallyUpdateNeeded = false;
volatile uint8_t xdata tallyUpdateData[4];
volatile uint8_t xdata backlightUpdateNeeded = false;
volatile uint8_t xdata backlightUpdateData[4];
volatile uint8_t xdata displayUpdateNeeded = false;
volatile uint8_t xdata displayUpdateData[34];

uint8_t xdata localTallyUpdateNeeded = false;
uint8_t xdata localTallyUpdateData[4];
uint8_t xdata localTallyState[4] = { 0, 0, 0, 0 };
uint8_t xdata localBacklightUpdateNeeded = false;
uint8_t xdata localBacklightUpdateData[4];
uint8_t xdata localDisplayUpdateNeeded = false;
uint8_t xdata localDisplayUpdateData[34];

SI_SEGMENT_VARIABLE(hex[16], const uint8_t, SI_SEG_CODE) = {
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
};

// RESET (?)
SI_SEGMENT_VARIABLE(pic1_init_pkt_1[3], const uint8_t, SI_SEG_CODE) = {
	0x81, 0x00,
	0xFE
};

// UNKNOWN
SI_SEGMENT_VARIABLE(pic1_init_pkt_2[45], const uint8_t, SI_SEG_CODE) = {
	0xA7, 0x00,
	0xA5, 0x00,
	0xA4, 0x02, 0x02, 0xA2,
	0xA4, 0x02, 0x42, 0xA2,
	0xA4, 0x02, 0x02, 0xA2,
	0xA4, 0x02, 0x42, 0xA2,
	0xA4, 0x02, 0x02, 0xA2,
	0xA4, 0x02, 0x42, 0xA2,
	0xA4, 0x02, 0x02, 0xA2,
	0xA4, 0x02, 0x42, 0xA2,
	0xA4, 0x02, 0x02, 0xA2,
	0xA4, 0x02, 0x42, 0xA2,
	0xFE
};

// UNKNOWN
SI_SEGMENT_VARIABLE(pic1_init_pkt_3[33], const uint8_t, SI_SEG_CODE) = {
	0xA4, 0x02, 0x02, 0xA2,
	0xA4, 0x02, 0x42, 0xA2,
	0xA4, 0x02, 0x02, 0xA2,
	0xA4, 0x02, 0x42, 0xA2,
	0xA4, 0x02, 0x02, 0xA2,
	0xA4, 0x02, 0x42, 0xA2,
	0xA4, 0x02, 0x02, 0xA2,
	0xA4, 0x02, 0x42, 0xA2,
	0xFE
};

// SOMETHING + ENABLE TALLY LIGHTS + SET CONTRAST TO 27
SI_SEGMENT_VARIABLE(pic1_init_pkt_4[14], const uint8_t, SI_SEG_CODE) = {
	0xE5, 0x03, 0x13, 0x50, 0x01,
	0xA4, 0x02, 0x83, 0x21,
	0xA4, 0x02, 0xC3, 0x21,
	0xFE
};

// index is "window" on the display then
// controller number to access that window then
// commands to set row 0, row 1, row 2, row 3 on that window and controller
SI_SEGMENT_VARIABLE(lcd_cmds[8][13], const uint8_t, SI_SEG_CODE) = {
    { 0, 0xb0, 0x11, 0x00, 0xb1, 0x11, 0x00, 0xb2, 0x11, 0x00, 0xb3, 0x11, 0x00 },
    { 0, 0xb0, 0x13, 0x00, 0xb1, 0x13, 0x00, 0xb2, 0x13, 0x00, 0xb3, 0x13, 0x00 },
    { 0, 0xb0, 0x15, 0x00, 0xb1, 0x15, 0x00, 0xb2, 0x15, 0x00, 0xb3, 0x15, 0x00 },
    { 1, 0xb0, 0x10, 0x05, 0xb1, 0x10, 0x05, 0xb2, 0x10, 0x05, 0xb3, 0x10, 0x05 },
    { 1, 0xb0, 0x12, 0x05, 0xb1, 0x12, 0x05, 0xb2, 0x12, 0x05, 0xb3, 0x12, 0x05 },
    { 1, 0xb0, 0x14, 0x05, 0xb1, 0x14, 0x05, 0xb2, 0x14, 0x05, 0xb3, 0x14, 0x05 },
    { 2, 0xb0, 0x10, 0x05, 0xb1, 0x10, 0x05, 0xb2, 0x10, 0x05, 0xb3, 0x10, 0x05 },
    { 2, 0xb0, 0x12, 0x05, 0xb1, 0x12, 0x05, 0xb2, 0x12, 0x05, 0xb3, 0x12, 0x05 }
};


//-----------------------------------------------------------------------------
// SiLabs_Startup() Routine
// ----------------------------------------------------------------------------
// This function is called immediately after reset, before the initialization
// code is run in SILABS_STARTUP.A51 (which runs before main() ). This is a
// useful place to disable the watchdog timer, which is enable by default
// and may trigger before main() in some instances.
//-----------------------------------------------------------------------------
void SiLabs_Startup (void)
{
  // Disable the watchdog here
}
 
// ----------------------------------------------------------------------------
// main() Routine
// ---------------------------------------------------------------------------
//
// Note: The software watchdog timer is not disabled by default in this
// example, so a long-running program will reset periodically unless
// the timer is disabled or your program periodically writes to it.
//
// Review the "Watchdog Timer" section under the part family's datasheet
// for details.
//
// ----------------------------------------------------------------------------
int16_t main(void)
{
	uint8_t a, b, i, j, d, ready;
	uint8_t SFRPAGE_save;
	uint8_t led_timer = 0;
    uint8_t newUsbState = 0;
    uint8_t oldUsbState = 0;

	enter_DefaultMode_from_RESET();

	// place crosspoint board in reset
	DATA = 0x00;
	RST_n = 0;
	A0 = 0;
	A1 = 0;
	A2 = 0;
	WR_n = 1;
	RD_n = 1;
	CSL_n = 1;
	CSR_n = 1;
	DIR = DIR_EFM2XPT;

	// Init Timer2 to generate interrupts at 100 Hz
	Timer2_Init (40000); // SYSCLK / 12 / 100

	// reset sequence
	RST_n = 0;
	for (i = 0; i < 10; i++) {
		while (!flag100) { };
		flag100 = false;
	}
	RST_n = 1;
	for (i = 0; i < 10; i++) {
		while (!flag100) { };
		flag100 = false;
	}

	// init right pic 1
	SendPicPacket (RIGHT_SIDE, PIC1, sizeof (pic1_init_pkt_1), pic1_init_pkt_1);
	WaitPicPacket ();
	SendPicPacket (RIGHT_SIDE, PIC1, sizeof (pic1_init_pkt_2), pic1_init_pkt_2);
	WaitPicPacket ();
	SendPicPacket (RIGHT_SIDE, PIC1, sizeof (pic1_init_pkt_3), pic1_init_pkt_3);
	WaitPicPacket ();
	SendPicPacket (RIGHT_SIDE, PIC1, sizeof (pic1_init_pkt_4), pic1_init_pkt_4);
	WaitPicPacket ();

	// initialize receive packet structure
    rxpktR1.rxpkt_state = 0;
	WaitPicPacket ();
	WaitPicPacket ();
	WaitPicPacket ();

    // reset lcd controller
    ResetLcd (RIGHT_SIDE, DISPLAY0);

    // initialize lcd controller
    InitLcd (RIGHT_SIDE, DISPLAY0);

    // clear lcd
    for (i = 0; i < 8; i++) {
    	DisplayImage (RIGHT_SIDE, DISPLAY0, i, FONT32X32[BIG_SPACE]);
    }

    // set all 16 backlight segments to amber
    SetLcdBacklights (RIGHT_SIDE, DISPLAY0, 0xFF, 0xFF, 0xFF, 0xFF);

	// main loop
	while (1)
	{
		// disable interrupts
		SFRPAGE_save = SFRPAGE;
		USB_DisableInts ();
		SFRPAGE = SFRPAGE_save;

		// handshake between USB and not USB stuff here

		// tally lights
		if (tallyUpdateNeeded) {
			localTallyUpdateNeeded = true;
			for (i = 0; i < 4; i++) {
				// re-order bytes so that bit 31 (far left bit) corresponds to tally 31
				// and bit 0 (far right bit) corresponds to tally 0
				localTallyUpdateData[i] = tallyUpdateData[3-i];
			}
			tallyUpdateNeeded = false;
		}

		// LCD backlight
		if (backlightUpdateNeeded) {
			localBacklightUpdateNeeded = true;
			for (i = 0; i < 4; i++) {
				localBacklightUpdateData[i] = backlightUpdateData[i];
			}
			backlightUpdateNeeded = false;
		}

		// LCD graphics and text
		if (displayUpdateNeeded) {
			localDisplayUpdateNeeded = true;
			for (i = 0; i < 34; i++) {
				localDisplayUpdateData[i] = displayUpdateData[i];
			}
			displayUpdateNeeded = false;
		}

		// re-enable interrupts
		SFRPAGE_save = SFRPAGE;
		USB_EnableInts ();
		SFRPAGE = SFRPAGE_save;

		// backlight update doesn't need PIC so doesn't have to be done on a 100 Hz tick
		if (localBacklightUpdateNeeded) {
			localBacklightUpdateNeeded = false;
			SetLcdBacklights (RIGHT_SIDE, PIC1, localBacklightUpdateData[0], localBacklightUpdateData[1], localBacklightUpdateData[2], localBacklightUpdateData[3]);
		}

		// text / graphics update doens't need PIC so doens't have to be done a 100 Hz tick
		// this board is always right pic, display 0
		if (localDisplayUpdateNeeded) {
			localDisplayUpdateNeeded = false;
			if (localDisplayUpdateData[0] == 0x05) {
				// text
				i = (localDisplayUpdateData[1] >> 2) & 7;

				switch (localDisplayUpdateData[1] & 3) {
					case 0:
						// 5x7
						DisplayText5x7 (RIGHT_SIDE, DISPLAY0, i, 0, &localDisplayUpdateData[2]);
						DisplayText5x7 (RIGHT_SIDE, DISPLAY0, i, 1, &localDisplayUpdateData[7]);
						DisplayText5x7 (RIGHT_SIDE, DISPLAY0, i, 2, &localDisplayUpdateData[12]);
						DisplayText5x7 (RIGHT_SIDE, DISPLAY0, i, 3, &localDisplayUpdateData[17]);
						break;
					case 1:
						// 8x8
						DisplayText8x8 (RIGHT_SIDE, DISPLAY0, i, 0, &localDisplayUpdateData[2]);
						DisplayText8x8 (RIGHT_SIDE, DISPLAY0, i, 1, &localDisplayUpdateData[6]);
						DisplayText8x8 (RIGHT_SIDE, DISPLAY0, i, 2, &localDisplayUpdateData[10]);
						DisplayText8x8 (RIGHT_SIDE, DISPLAY0, i, 3, &localDisplayUpdateData[14]);
						break;
					case 2:
						// 10x14
						DisplayText10x14 (RIGHT_SIDE, DISPLAY0, i, 0, &localDisplayUpdateData[2]);
						DisplayText10x14 (RIGHT_SIDE, DISPLAY0, i, 2, &localDisplayUpdateData[5]);
						break;
					case 3:
						// 32x32
						j = localDisplayUpdateData[2];
						if (j > BIG_SPACE) {
							j = BIG_SPACE;
						}
				    	DisplayImage (RIGHT_SIDE, DISPLAY0, i, FONT32X32[j]);
						break;
				}
			} else if (localDisplayUpdateData[0] == 0x06) {
				i = (localDisplayUpdateData[1] >> 2) & 7;
				DisplayImageRow (RIGHT_SIDE, DISPLAY0, i, localDisplayUpdateData[1] & 3, &localDisplayUpdateData[2]);
			}
		}

		// check if 100 Hz timer expired, if so, reset flag and do 100 Hz tasks
		if (flag100) {
			flag100 = false;

			// get usb state
			newUsbState = USBD_GetUsbState ();

			// blink user LED based on USB state
			if (newUsbState == USBD_STATE_CONFIGURED) {
				// run one led pattern for configured usb state
				if (led_timer < 25) {
					LED = 0; // LED ON
				} else if (led_timer < 50) {
					LED = 1; // LED OFF
				} else if (led_timer < 75) {
					LED = 0; // LED ON
				} else if (led_timer < 100) {
					LED = 1; // LED OFF
				}

				// clear lamps on entering configured state
				if (newUsbState != oldUsbState) {
					oldUsbState = newUsbState;
					// do stuff on entering configured state here
					// TODO
				}
			} else {
				// and another for all other usb states
				if (led_timer < 25) {
					LED = 0; // LED ON
				} else if (led_timer < 50) {
					LED = 1; // LED OFF
				} else if (led_timer < 75) {
					LED = 1; // LED ON
				} else if (led_timer < 100) {
					LED = 1; // LED OFF
				}

				// clear lamps on leaving configured state
				if ((newUsbState != oldUsbState) && (oldUsbState == USBD_STATE_CONFIGURED)) {
					oldUsbState = newUsbState;
					// do stuff on exiting configured state here
					// TODO
				}
			}

			// read packet from right PIC 1 at 100 Hz
			for (i = 0; i < 32; i++) {
				d = ReadPic (RIGHT_SIDE, PIC1);
				ready = ReceivePicPacket (&rxpktR1, d);
				if (ready) {
					if ((rxpktR1.rxpkt_length == 0x0F) && (rxpktR1.rxpkt_buffer[0] == 0x91)) {
						// got a key press packet
						for (j = 0; j < 8; j++) {
							keyReportData[j] = rxpktR1.rxpkt_buffer[1+j];
						}
						keyReportNeeded = true;
					}
					break;
				}
			}

			if (localTallyUpdateNeeded) {
				j = 0;
				for (i = 0; i < 32; i++) {
					// order of bytes are swapped when copied from USB rx data
					// so that bit 0 corresponds to tally light 0
					a = localTallyUpdateData[i >> 3] & (0x01 << (i & 7));
					b = localTallyState[i >> 3] & (0x01 << (i & 7));
					if (a != b) {
						txpkt[j++] = 0xA6;
						txpkt[j++] = 0x02;
						txpkt[j++] = i;
						txpkt[j++] = a ? 0x07 : 0x00;
						localTallyState[i >> 3] ^= (0x01 << (i & 7));
					}
					if (j >= 32) {
						break;
					}
				}
				if (j != 0) {
					txpkt[j++] = 0xFE;
					SendPicPacket (RIGHT_SIDE, PIC1, j, txpkt);
				} else {
					// stop scanning if no more changes to make
					localTallyUpdateNeeded = false;
				}
			}


/*
			// blink tally lights sequentially
			if ((led_timer == 0) || (led_timer == 25) || (led_timer == 50) || (led_timer == 75)) {
				if (tallyLight == 0) {
			    	DisplayImage (RIGHT_SIDE, DISPLAY0, 0, FONT32X32[BIG_A]);
			    	DisplayImage (RIGHT_SIDE, DISPLAY0, 1, FONT32X32[BIG_W]);
			    	DisplayImage (RIGHT_SIDE, DISPLAY0, 2, FONT32X32[BIG_E]);
			    	DisplayImage (RIGHT_SIDE, DISPLAY0, 3, FONT32X32[BIG_S]);
			    	DisplayImage (RIGHT_SIDE, DISPLAY0, 4, FONT32X32[BIG_O]);
			    	DisplayImage (RIGHT_SIDE, DISPLAY0, 5, FONT32X32[BIG_M]);
			    	DisplayImage (RIGHT_SIDE, DISPLAY0, 6, FONT32X32[BIG_E]);
			    	DisplayImage (RIGHT_SIDE, DISPLAY0, 7, FONT32X32[BIG_SPACE]);
				} else if (tallyLight == 8) {
			    	DisplayImage (RIGHT_SIDE, DISPLAY0, 0, FONT32X32[BIG_SPACE]);
			    	DisplayImage (RIGHT_SIDE, DISPLAY0, 1, FONT32X32[BIG_SPACE]);
			    	DisplayImage (RIGHT_SIDE, DISPLAY0, 2, FONT32X32[BIG_S]);
			    	DisplayImage (RIGHT_SIDE, DISPLAY0, 3, FONT32X32[BIG_A]);
			    	DisplayImage (RIGHT_SIDE, DISPLAY0, 4, FONT32X32[BIG_U]);
			    	DisplayImage (RIGHT_SIDE, DISPLAY0, 5, FONT32X32[BIG_C]);
			    	DisplayImage (RIGHT_SIDE, DISPLAY0, 6, FONT32X32[BIG_E]);
			    	DisplayImage (RIGHT_SIDE, DISPLAY0, 7, FONT32X32[BIG_EXCLAM]);
	                DisplayText5x7 (RIGHT_SIDE, DISPLAY0, 0, 0, "ABCDE");
	                DisplayText5x7 (RIGHT_SIDE, DISPLAY0, 0, 1, "12345");
	                DisplayText5x7 (RIGHT_SIDE, DISPLAY0, 0, 2, "54321");
	                DisplayText5x7 (RIGHT_SIDE, DISPLAY0, 0, 3, "EDCBA");
	                DisplayText8x8 (RIGHT_SIDE, DISPLAY0, 1, 0, "ABCD");
	                DisplayText8x8 (RIGHT_SIDE, DISPLAY0, 1, 1, "1234");
	                DisplayText8x8 (RIGHT_SIDE, DISPLAY0, 1, 2, "5432");
	                DisplayText8x8 (RIGHT_SIDE, DISPLAY0, 1, 3, "EDCB");
				} else if (tallyLight == 16) {
			    	DisplayImage (RIGHT_SIDE, DISPLAY0, 0, FONT32X32[BIG_EXCLAM]);
			    	DisplayImage (RIGHT_SIDE, DISPLAY0, 1, FONT32X32[BIG_D]);
			    	DisplayImage (RIGHT_SIDE, DISPLAY0, 2, FONT32X32[BIG_A]);
			    	DisplayImage (RIGHT_SIDE, DISPLAY0, 3, FONT32X32[BIG_N]);
			    	DisplayImage (RIGHT_SIDE, DISPLAY0, 4, FONT32X32[BIG_G]);
			    	DisplayImage (RIGHT_SIDE, DISPLAY0, 5, FONT32X32[BIG_E]);
			    	DisplayImage (RIGHT_SIDE, DISPLAY0, 6, FONT32X32[BIG_R]);
			    	DisplayImage (RIGHT_SIDE, DISPLAY0, 7, FONT32X32[BIG_EXCLAM]);
				} else if (tallyLight == 24) {
			    	DisplayImage (RIGHT_SIDE, DISPLAY0, 0, FONT32X32[BIG_Z]);
			    	DisplayImage (RIGHT_SIDE, DISPLAY0, 1, FONT32X32[BIG_O]);
			    	DisplayImage (RIGHT_SIDE, DISPLAY0, 2, FONT32X32[BIG_M]);
			    	DisplayImage (RIGHT_SIDE, DISPLAY0, 3, FONT32X32[BIG_B]);
			    	DisplayImage (RIGHT_SIDE, DISPLAY0, 4, FONT32X32[BIG_I]);
			    	DisplayImage (RIGHT_SIDE, DISPLAY0, 5, FONT32X32[BIG_E]);
			    	DisplayImage (RIGHT_SIDE, DISPLAY0, 6, FONT32X32[BIG_S]);
			    	DisplayImage (RIGHT_SIDE, DISPLAY0, 7, FONT32X32[BIG_EXCLAM]);
				}
			}
*/

			if (newUsbState != USBD_STATE_CONFIGURED) {
				// do other stuff while not configured for USB here
				// TODO
    		}

			// update led timer
			if (++led_timer == 100) {
				led_timer = 0;
			}

		} // end if (flag100)

	} // end while (1)
}


void Timer2_Init (int counts)
{
   TMR2CN0 = 0x00;                     // Stop Timer2; Clear TF2;
                                       // use SYSCLK/12 as timebase
   CKCON0 &= ~0x60;                    // Timer2 clocked based on T2XCLK;

   TMR2RL = -counts;                   // Init reload values
   TMR2 = 0xffff;                      // Set to reload immediately
   IE_ET2 = 1;                         // Enable Timer2 interrupts
   TMR2CN0_TR2 = 1;                    // Start Timer2
}

SI_INTERRUPT(TIMER2_ISR, TIMER2_IRQn)
{
    TMR2CN0_TF2H = 0;                   // Clear Timer2 interrupt flag
	flag100 = 1;
}


void SendPicPacket (uint8_t side, uint8_t pic, uint8_t length, uint8_t *dpkt)
{
	uint8_t i, cksum;

	WritePic (side, pic, 0x80);
	cksum = 0x80;
	WritePic (side, pic, length);
	cksum += length;
	for (i = 0; i < length; i++) {
		WritePic (side, pic, dpkt[i]);
		cksum += dpkt[i];
	}
	WritePic (side, pic, cksum);
}


void WaitPicPacket (void)
{
	volatile uint8_t j, k;

	for (j = 0; j < 50; j++) {
		for (k = 0; k < 200; k++) {
		};
	}
}


void WritePic (uint8_t side, uint8_t pic, uint8_t d)
{
	volatile uint8_t i;

	// make sure control lines are in a known state
	WR_n = 1;
	RD_n = 1;
	CSL_n = 1;
	CSR_n = 1;

	// setup address and data
	A0 = (pic & 0x01) ? 1 : 0;
	A1 = (pic & 0x02) ? 1 : 0;
	A2 = (pic & 0x04) ? 1 : 0;
	DIR = DIR_EFM2XPT;          // 8T245 = EFM8 --> XPT
	P1MDOUT = 0xFF;             // EFM8 P1 push-pull
	P1 = d;                     // data

	// for (i = 0; i < 10; i++) { };

	// assert write low
	WR_n = 0;

	for (i = 0; i < 2; i++) { };

	// assert CSx_n
	if (side == LEFT_SIDE) {
		CSL_n = 0;
	} else if (side == RIGHT_SIDE) {
		CSR_n = 0;
	}

	for (i = 0; i < 2; i++) { };

	// deassert CSx_n's
	CSL_n = 1;
	CSR_n = 1;

	for (i = 0; i < 2; i++) { };

	// deassert WR_n
	WR_n = 1;

	for (i = 0; i < 75; i++) { };
}


uint8_t ReadPic (uint8_t side, uint8_t pic)
{
	volatile uint8_t i, d;

	// make sure control lines are in a known state
	WR_n = 1;
	RD_n = 1;
	CSL_n = 1;
	CSR_n = 1;

	// setup address and data
	A0 = (pic & 0x01) ? 1 : 0;
	A1 = (pic & 0x02) ? 1 : 0;
	A2 = (pic & 0x04) ? 1 : 0;
	P1MDOUT = 0x00;             // EFM8 P1 open-drain
	P1 = 0xFF;                  // float high
	DIR = DIR_XPT2EFM;          // 8T245 = XPT --> EFM

	// assert read low
	RD_n = 0;

	for (i = 0; i < 2; i++) { };

	// assert CSx_n
	if (side == LEFT_SIDE) {
		CSL_n = 0;
	} else if (side == RIGHT_SIDE) {
		CSR_n = 0;
	}

	for (i = 0; i < 2; i++) { };

	// read data
	d = P1;

	// deassert CSx_n's
	CSL_n = 1;
	CSR_n = 1;

	for (i = 0; i < 2; i++) { };

	// deassert RD_n
	RD_n = 1;

	// reverse data bus direction
	DIR = DIR_EFM2XPT;          // 8T245 = EFM --> XPT
	P1MDOUT = 0xFF;             // EFM8 P1 push pull

	for (i = 0; i < 75; i++) { };

	return d;
}


uint8_t ReceivePicPacket (rxpkt_t *rx, uint8_t d)
{
	if (rx->rxpkt_state == 0) {
		// get start of packet
		if (d == 0x80) {
			rx->rxpkt_state++;
			rx->rxpkt_cksum = 0x80;
			rx->rxpkt_length = 0;
			rx->rxpkt_count = 0;
		}
	} else if (rx->rxpkt_state == 1) {
		// get length of packet, reject long packets
		if (d <= 32) {
			rx->rxpkt_state++;
			rx->rxpkt_cksum += d;
			rx->rxpkt_length = d;
			rx->rxpkt_count = 0;
		} else {
			rx->rxpkt_state = 0;
		}
	} else if (rx->rxpkt_state == 2) {
		// get length bytes into buffer
		rx->rxpkt_buffer[rx->rxpkt_count++] = d;
		rx->rxpkt_cksum += d;
		if (rx->rxpkt_length == rx->rxpkt_count) {
			if (d == 0xFE) {
				rx->rxpkt_state++;
			} else {
				rx->rxpkt_state = 0;
			}
		}
	} else if (rx->rxpkt_state == 3) {
		// get and compare checksum
		rx->rxpkt_state = 0;
		if (rx->rxpkt_cksum == d) {
			return true;
		}
	}

	return false;
}


void SetTallyLight (uint8_t side, uint8_t pic, uint8_t tally)
{
	txpkt[0] = 0xa6;
	txpkt[1] = 0x02;
	txpkt[2] = tally;
	txpkt[3] = 0x07;
	txpkt[4] = 0xfe;
	SendPicPacket (side, pic, 5, txpkt);
}


void ClearTallyLight (uint8_t side, uint8_t pic, uint8_t tally)
{
	txpkt[0] = 0xa6;
	txpkt[1] = 0x02;
	txpkt[2] = tally;
	txpkt[3] = 0x00;
	txpkt[4] = 0xfe;
	SendPicPacket (side, pic, 5, txpkt);
}


void ClearSetTallyLights (uint8_t side, uint8_t pic, uint8_t clear, uint8_t set)
{
	txpkt[0] = 0xa6;
	txpkt[1] = 0x02;
	txpkt[2] = clear;
	txpkt[3] = 0x00;
	txpkt[4] = 0xa6;
	txpkt[5] = 0x02;
	txpkt[6] = set;
	txpkt[7] = 0x07;
	txpkt[8] = 0xfe;
	SendPicPacket (side, pic, 9, txpkt);
}


void WriteFpgaBegin (void)
{
	// make sure control lines are in a known state
	WR_n = 1;
	RD_n = 1;
	CSL_n = 1;
	CSR_n = 1;

	// setup address and data
	DIR = DIR_EFM2XPT;          // 8T245 = EFM8 --> XPT
	P1MDOUT = 0xFF;             // EFM8 P1 push-pull
}


void WriteFpga (uint8_t side, uint8_t a, uint8_t d)
{
	// make sure control lines are in a known state

	// setup address and data
	A0 = (a & 0x01) ? 1 : 0;
	A1 = (a & 0x02) ? 1 : 0;
	A2 = (a & 0x04) ? 1 : 0;
	P1 = d;

	// assert write low
	WR_n = 0;

	// assert CSx_n
	if (side == LEFT_SIDE) {
		CSL_n = 0;
	} else if (side == RIGHT_SIDE) {
		CSR_n = 0;
	}

	// deassert CSx_n's
	CSL_n = 1;
	CSR_n = 1;

	// deassert WR_n
	WR_n = 1;
}


void SetLcdBacklights (uint8_t side, uint8_t display, uint8_t redtop, uint8_t redbot, uint8_t grntop, uint8_t grnbot)
{
    int i;
    uint8_t d;

	// set up for FPGA writes
    WriteFpgaBegin ();

    // WriteFpga (side, LCD_DATA, 0xc0);             // put d7,d6 back in a safe state before switching to backlight
    WriteFpga (side, LCD_SELECT, display | 0x03); // select display number and backlights

    // clock out data serially
    for (i = 0; i < 8; i++) {
        d = 0;
        d |= (redtop & 0x80) ? 8 : 0;
        d |= (redbot & 0x80) ? 4 : 0;
        d |= (grntop & 0x80) ? 2 : 0;
        d |= (grnbot & 0x80) ? 1 : 0;
        // WriteFpga (side, LCD_DATA, 0xc0 | d);
        WriteFpga (side, LCD_DATA, 0x40 | d);
        // WriteFpga (side, LCD_DATA, 0xc0 | d);
        redtop <<= 1;
        redbot <<= 1;
        grntop <<= 1;
        grnbot <<= 1;
    }

    // create high pulse on A6275 latch
    // WriteFpga (side, LCD_DATA, 0xc0);
    WriteFpga (side, LCD_DATA, 0x80);
    // WriteFpga (side, LCD_DATA, 0xc0);
}


void ResetLcd (uint8_t side, uint8_t display)
{
    WriteFpgaBegin ();
	WriteFpga (side, LCD_SELECT, display | 0x03);
	// WriteFpga (side, LCD_CMD, 0xc0);
	WriteFpga (side, LCD_CMD, 0x00);
	// WriteFpga (side, LCD_CMD, 0xc0);
}


void InitLcd (uint8_t side, uint8_t display)
{
	// set up for FPGA writes
    WriteFpgaBegin ();

    // select display and controller 0
	WriteFpga (side, LCD_SELECT, display | 0x00);

    // write command registers
	WriteFpga (side, LCD_CMD, 0xAE);         // display off
	WriteFpga (side, LCD_CMD, 0x40);         // page address
    WriteFpga (side, LCD_CMD, 0xA1);         // scan direction = reversed
    WriteFpga (side, LCD_CMD, 0xA6);         // inverse display = normal
    WriteFpga (side, LCD_CMD, 0xA4);         // all pixel on = normal
    WriteFpga (side, LCD_CMD, 0xA3);         // bias = 1/7
    WriteFpga (side, LCD_CMD, 0xC8);         // com direction = reversed
    WriteFpga (side, LCD_CMD, 0x2D);         // power control = VB, VF
    WriteFpga (side, LCD_CMD, 0xAC);
    WriteFpga (side, LCD_CMD, 0xAF);         // display on

	// select display and controller 1
    WriteFpga (side, LCD_SELECT, display | 0x01);

    // write command registers
    WriteFpga (side, LCD_CMD, 0xAE);
    WriteFpga (side, LCD_CMD, 0x40);
    WriteFpga (side, LCD_CMD, 0xA1);
    WriteFpga (side, LCD_CMD, 0xA6);
    WriteFpga (side, LCD_CMD, 0xA4);
    WriteFpga (side, LCD_CMD, 0xA2);
    WriteFpga (side, LCD_CMD, 0x28);
    WriteFpga (side, LCD_CMD, 0xAC);
    WriteFpga (side, LCD_CMD, 0xAF);

	// select display and controller 2
    WriteFpga (side, LCD_SELECT, display | 0x02);

    // write command registers
    WriteFpga (side, LCD_CMD, 0xAE);
    WriteFpga (side, LCD_CMD, 0x40);
    WriteFpga (side, LCD_CMD, 0xA1);
    WriteFpga (side, LCD_CMD, 0xA6);
    WriteFpga (side, LCD_CMD, 0xA4);
    WriteFpga (side, LCD_CMD, 0xA2);
    WriteFpga (side, LCD_CMD, 0x28);
    WriteFpga (side, LCD_CMD, 0xAC);
    WriteFpga (side, LCD_CMD, 0xAF);
}


void DisplayImage (uint8_t side, uint8_t display, uint8_t window, uint8_t *image)
{
	uint8_t i, j;

	// set up for FPGA writes
    WriteFpgaBegin ();

	WriteFpga (side, LCD_SELECT, display | lcd_cmds[window][0]);

	for (i = 0; i < 4; i++) {
		WriteFpga (side, LCD_CMD, lcd_cmds[window][3*i+1]);
		WriteFpga (side, LCD_CMD, lcd_cmds[window][3*i+2]);
		WriteFpga (side, LCD_CMD, lcd_cmds[window][3*i+3]);
		for (j = 0; j < 32; j++) {
			WriteFpga (side, LCD_DATA, *image++);
		}
	}
}


void DisplayImageRow (uint8_t side, uint8_t display, uint8_t window, uint8_t row, uint8_t *image)
{
	uint8_t j;

	// set up for FPGA writes
    WriteFpgaBegin ();

	WriteFpga (side, LCD_SELECT, display | lcd_cmds[window][0]);

	WriteFpga (side, LCD_CMD, lcd_cmds[window][3*row+1]);
	WriteFpga (side, LCD_CMD, lcd_cmds[window][3*row+2]);
	WriteFpga (side, LCD_CMD, lcd_cmds[window][3*row+3]);
	for (j = 0; j < 32; j++) {
		WriteFpga (side, LCD_DATA, *image++);
	}
}


void DisplayText5x7 (uint8_t side, uint8_t display, uint8_t window, uint8_t row, char *text)
{
    uint8_t i, j;

	// set up for FPGA writes
    WriteFpgaBegin ();

    // pick window
    WriteFpga (side, LCD_SELECT, display | lcd_cmds[window][0]);

    // pick row
    WriteFpga (side, LCD_CMD, lcd_cmds[window][3*row+1]);
    WriteFpga (side, LCD_CMD, lcd_cmds[window][3*row+2]);
    WriteFpga (side, LCD_CMD, lcd_cmds[window][3*row+3]);

    // write text data
    WriteFpga (side, LCD_DATA, 0x00);
    for (i = 0; i < 5; i++) {
        for (j = 0; j < 5; j++) {
        	WriteFpga (side, LCD_DATA, FONT5X7[text[i]-32][j]);
        }
        WriteFpga (side, LCD_DATA, 0x00);
    }
    WriteFpga (side, LCD_DATA, 0x00);
}


void DisplayText8x8 (uint8_t side, uint8_t display, uint8_t window, uint8_t row, char *text)
{
    uint8_t i, j;

	// set up for FPGA writes
    WriteFpgaBegin ();

    // pick window
    WriteFpga (side, LCD_SELECT, display | lcd_cmds[window][0]);

    // pick row
    WriteFpga (side, LCD_CMD, lcd_cmds[window][3*row+1]);
    WriteFpga (side, LCD_CMD, lcd_cmds[window][3*row+2]);
    WriteFpga (side, LCD_CMD, lcd_cmds[window][3*row+3]);

    // write text data
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 8; j++) {
        	WriteFpga (side, LCD_DATA, FONT8X8[text[i]-32][j]);
        }
    }
}


void DisplayText10x14 (uint8_t side, uint8_t display, uint8_t window, uint8_t row, char *text)
{
	uint8_t i, j;

	// set up for FPGA writes
    WriteFpgaBegin ();

	// pick window
    WriteFpga (side, LCD_SELECT, display | lcd_cmds[window][0]);

    // pick row
    WriteFpga (side, LCD_CMD, lcd_cmds[window][3*row+1]);
    WriteFpga (side, LCD_CMD, lcd_cmds[window][3*row+2]);
    WriteFpga (side, LCD_CMD, lcd_cmds[window][3*row+3]);

    // write text data
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 10; j++) {
        	WriteFpga (side, LCD_DATA, FONT10X14[text[i]-32][j] << 1);
        }
        if (i < 2) {
        	WriteFpga (side, LCD_DATA, 0x00);
        }
    }

    // pick row + 1
    row++;
    WriteFpga (side, LCD_CMD, lcd_cmds[window][3*row+1]);
    WriteFpga (side, LCD_CMD, lcd_cmds[window][3*row+2]);
    WriteFpga (side, LCD_CMD, lcd_cmds[window][3*row+3]);

    // write text data
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 10; j++) {
        	WriteFpga (side, LCD_DATA, FONT10X14[text[i]-32][j] >> 7);
        }
        if (i < 2) {
        	WriteFpga (side, LCD_DATA, 0x00);
        }
    }
}

