//-----------------------------------------------------------------------------------------------
// EFM8UB2 USB Controller Software for Grass Valley Series 7000 Panels
//

//-----------------------------------------------------------------------------------------------
// includes
//

#include "SI_EFM8UB2_Register_Enums.h"
#include "efm8_usb.h"
#include "descriptors.h"
#include "idle.h"
#include "InitDevice.h"
#include "config.h"


//-----------------------------------------------------------------------------------------------
// defines
//

#define LED_ON  0
#define LED_OFF 1
#define SYSCLK       48000000      // SYSCLK frequency in Hz


//-----------------------------------------------------------------------------------------------
// typedefs
//


//-----------------------------------------------------------------------------------------------
// prototypes
//

void Timer2_Init (int counts);
void SetLamp (uint8_t row, uint8_t lamp);
uint8_t GetRow (uint8_t row);


//-----------------------------------------------------------------------------------------------
// globals
//

// signals from ISRs to main loop
volatile uint8_t flag100 = 0;

volatile uint8_t needSend;
volatile uint8_t sendData[2];

volatile uint8_t updateNeeded;
volatile uint8_t updateRow;
volatile uint8_t updateCol;

uint8_t newUpdate;
uint8_t newRow;
uint8_t newCol;

uint8_t key[3] = { 0, 0, 0 };
uint8_t state[3] = { 0, 0, 0 };


//-----------------------------------------------------------------------------------------------
// SiLabs_Startup() Routine
//

void SiLabs_Startup (void)
{
  // Disable the watchdog here
}
 

//-----------------------------------------------------------------------------------------------
// main() Routine
//

int16_t main(void)
{
	uint8_t SFRPAGE_save;
	uint8_t led_timer = 0;
	uint8_t newUsbState = 0;
	uint8_t oldUsbState = 0;
	uint8_t lampNumber = 0xff;
	uint8_t row, a, b;

	// uint8_t lamp, display, digit;
	// volatile uint16_t delayCounter;

	enter_DefaultMode_from_RESET();

	// Init Timer2 to generate interrupts at 100 Hz
	Timer2_Init (40000); // SYSCLK / 12 / 100

	needSend = false;
	sendData[0] = 0;
	sendData[1] = 0;

	updateNeeded = false;
	updateRow = 0;
	updateCol = 0;

	newUpdate = false;
	newRow = 0;
	newCol = 0;;

	SetLamp (0, key[0]);
	SetLamp (1, key[1]);
	SetLamp (2, key[2]);

	// loop forever
	while (1)
	{
		SFRPAGE_save = SFRPAGE;
		USB_DisableInts ();
		SFRPAGE = SFRPAGE_save;

		if (updateNeeded) {
			updateNeeded = false;
			newUpdate = true;
			newRow = updateRow;
			newCol = updateCol;
		}

		SFRPAGE_save = SFRPAGE;
		USB_EnableInts ();
		SFRPAGE = SFRPAGE_save;

		if (newUpdate) {
			newUpdate = false;
			if (newRow <= 2) {
				key[newRow] = newCol;
				state[newRow] = 0;
				SetLamp (newRow, newCol);
			}
		}

		// check if 100 Hz timer expired
		if (flag100) {
			flag100 = false;

			// new usb state
			newUsbState = USBD_GetUsbState ();

			if (newUsbState == USBD_STATE_CONFIGURED) {
				// run one led pattern for configured usb state
				if (led_timer < 25) {
					LED = LED_ON; // LED ON
				} else if (led_timer < 50) {
					LED = LED_OFF; // LED OFF
				} else if (led_timer < 75) {
					LED = LED_ON; // LED ON
				} else if (led_timer < 100) {
					LED = LED_OFF; // LED OFF
				}

				// clear lamps on entering configured state
				if (newUsbState != oldUsbState) {
					oldUsbState = newUsbState;
					for (row = 0; row < 3; row++) {
						key[row] = 0xff;
						state[row] = 0;
						SetLamp (row, key[row]);
					}
				}
			} else {
				// and another for all other usb states
				if (led_timer < 50) {
					LED = LED_ON; // LED ON
				} else {
					LED = LED_OFF; // LED OFF
				}
				// clear lamps on leaving configured state
				if ((newUsbState != oldUsbState) && (oldUsbState == USBD_STATE_CONFIGURED)) {
					oldUsbState = newUsbState;
					for (row = 0; row < 3; row++) {
						key[row] = 0xff;
						state[row] = 0;
						SetLamp (row, key[row]);
					}
				}
			}

			if (newUsbState == USBD_STATE_CONFIGURED) {
				for (row = 0; row < 3; row++) {
					b = GetRow (row);
					switch (state[row]) {
						case 0:
							// if key is down, turn off lamps so we can read it and go to the next state
							if ((b & 1) == 0) {
								SetLamp (row, 0xFF);
								state[row] = 1;
							}
							break;
						case 1:
							// if key is still down, read it and set that as the new key
							// when connected to usb, send the new key to host
							// if key is not still down, set the lamps back to the old value
							if ((b & 1) == 0) {
								if ((b & G0) == 0) {
									a = 16 + ((b >> 1) & 0x7);
								} else if ((b & G1) == 0) {
									a =  8 + ((b >> 1) & 0x7);
								} else if ((b & G2) == 0) {
									a =  0 + ((b >> 1) & 0x7);
								}
								key[row] = a;
								SetLamp (row, key[row]);
								state[row] = 2;
								sendData[0] = row;
								sendData[1] = key[row];
								needSend = true;
							} else {
								SetLamp (row, key[row]);
								state[row] = 0;
							}
							break;
						case 2:
							// wait for key to be released
							if ((b & 1) == 1) {
								state[row] = 0;
							}
							break;
					}
				}
			}

			if (newUsbState != USBD_STATE_CONFIGURED) {
				// run usb not configured idle pattern here
				if ((led_timer == 0) || (led_timer == 25) || (led_timer == 50) || (led_timer == 75)) {
					lampNumber++;
					if (lampNumber >= 24) {
						lampNumber = 0;
					}
					SetLamp (2, lampNumber);
					a = lampNumber - 1;
					if (a > 24) a += 24;
					SetLamp (1, a);
					a = lampNumber - 2;
					if (a > 24) a += 24;
					SetLamp (0, a);
				}
			}

			// update 1 second led timer
			if (++led_timer == 100) {
				led_timer = 0;
			}

		} // end if (flag100)
	}
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


#if 0
uint16_t lfsr (void)
{
    uint16_t newbit;

    newbit = ((lfsr_state >> 0) ^ (lfsr_state >> 2) ^ (lfsr_state >> 3) ^ (lfsr_state >> 5)) /* & 1u */;
    lfsr_state = (lfsr_state >> 1) | (newbit << 15);

    return lfsr_state;
}
#endif


void SetLamp (uint8_t row, uint8_t lamp)
{
	uint8_t maskval = 0x7f; // bits we may change
	uint8_t outval = 0x71;  // all lamps off, E0 high

	if ((lamp >= 0) && (lamp <= 7)) {
		outval &= ~G2;
	} else if ((lamp >= 8) && (lamp <= 15)) {
		outval &= ~G1;
	} else if ((lamp >= 16) && (lamp <= 23)) {
		outval &= ~G0;
	}

	outval |= (lamp & 7) << 1;

	if (row == 0) {
		ROW1 = outval;
	} else if (row == 1) {
		ROW2 = outval;
	} else if (row == 2) {
		ROW3 = outval;
	}
}


uint8_t GetRow (uint8_t row)
{
	if (row == 0) {
		return ROW1;
	} else if (row == 1) {
		return ROW2;
	} else if (row == 2) {
		return ROW3;
	}

	return 0;
}
