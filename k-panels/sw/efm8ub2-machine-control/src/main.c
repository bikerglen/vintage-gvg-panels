//=============================================================================================
// INCLUDES
//

#include "SI_EFM8UB2_Register_Enums.h"
#include "efm8_usb.h"
#include "descriptors.h"
#include "idle.h"
#include "InitDevice.h"


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

#define MAX_PIC_RX_PKT_LENGTH   16


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

void SetDisplayText (uint8_t *text);


//=============================================================================================
// GLOBALS
//

volatile uint8_t flag100 = 0;

uint8_t xdata txpkt[48];

rxpkt_t xdata rxpktR1;

volatile uint8_t xdata keyReportNeeded = false;
volatile uint8_t xdata keyReportData[8];
volatile uint8_t xdata displayUpdateNeeded = false;
volatile uint8_t xdata displayUpdateData[12];
volatile uint8_t xdata tallyUpdateNeeded = false;
volatile uint8_t xdata tallyUpdateData[4];

uint8_t xdata localDisplayUpdateNeeded = false;
uint8_t xdata localDisplayUpdateData[12];
uint8_t xdata localTallyUpdateNeeded = false;
uint8_t xdata localTallyUpdateData[4];
uint8_t xdata localTallyState[4] = { 0, 0, 0, 0 };

SI_SEGMENT_VARIABLE(hex[16], const uint8_t, SI_SEG_CODE) = {
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
};

SI_SEGMENT_VARIABLE(pic1_init_pkt_1[3], const uint8_t, SI_SEG_CODE) = {
	0x81, 0x00, 0xFE
};

SI_SEGMENT_VARIABLE(pic1_init_pkt_2[9], const uint8_t, SI_SEG_CODE) = {
	0xA4, 0x02, 0x83, 0x21, 0xA4, 0x02, 0xC3, 0x21, 0xFE
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

	// place machine control board in reset
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

	// initialize receive packet structure
    rxpktR1.rxpkt_state = 0;

	// main loop
	while (1)
	{
		// disable interrupts
		SFRPAGE_save = SFRPAGE;
		USB_DisableInts ();
		SFRPAGE = SFRPAGE_save;

		// handshake between USB and not USB stuff here

		// display
		if (displayUpdateNeeded) {
			localDisplayUpdateNeeded = true;
			for (i = 0; i < 12; i++) {
				localDisplayUpdateData[i] = displayUpdateData[i];
			}
			displayUpdateNeeded = false;
		}

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

		// re-enable interrupts
		SFRPAGE_save = SFRPAGE;
		USB_EnableInts ();
		SFRPAGE = SFRPAGE_save;

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
			for (i = 0; i < (MAX_PIC_RX_PKT_LENGTH+3); i++) {
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

			if (localDisplayUpdateNeeded) {
				localDisplayUpdateNeeded = false;
				SetDisplayText (localDisplayUpdateData);
			} else if (localTallyUpdateNeeded) {
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


void SetDisplayText (uint8_t *text)
{
	int i;

	txpkt[0] = 0xA1;
	txpkt[1] = 0x0C;
	for (i = 0; i < 12; i++) {
		txpkt[2+i] = text[i];
	}
	txpkt[14] = 0xFE;
	SendPicPacket (RIGHT_SIDE, PIC1, 15, txpkt);
}
