/**************************************************************************//**
 * Copyright (c) 2015 by Silicon Laboratories Inc. All rights reserved.
 *
 * http://developer.silabs.com/legal/version/v11/Silicon_Labs_Software_License_Agreement.txt
 *****************************************************************************/

#include "SI_EFM8UB2_Register_Enums.h"
#include "efm8_usb.h"
#include "descriptors.h"
#include "idle.h"

#define USBD_READ_SIZE 34

uint8_t tmpBuffer;
uint8_t xdata txBuffer[64];
uint8_t xdata rxBuffer[64];

extern volatile uint8_t xdata keyReportNeeded;
extern volatile uint8_t xdata keyReportData[8];
extern volatile uint8_t xdata tallyUpdateNeeded;
extern volatile uint8_t xdata tallyUpdateData[4];
extern volatile uint8_t xdata backlightUpdateNeeded;
extern volatile uint8_t xdata backlightUpdateData[4];
extern volatile uint8_t xdata displayUpdateNeeded;
extern volatile uint8_t xdata displayUpdateData[34];



// ----------------------------------------------------------------------------
// Functions
// ----------------------------------------------------------------------------

void USBD_ResetCb (void)
{
}

void USBD_SofCb(uint16_t sofNr)
{
	UNREFERENCED_ARGUMENT(sofNr);

	idleTimerTick();

	// Check if the device should send a report
	if (isIdleTimerExpired() == true) {
	    if (USBD_EpIsBusy(EP1IN) == false) {
	    	if (keyReportNeeded) {
			    txBuffer[0] = 0x01;
  			    txBuffer[1] = keyReportData[0];
	  		    txBuffer[2] = keyReportData[1];
			    txBuffer[3] = keyReportData[2];
			    txBuffer[4] = keyReportData[3];
			    txBuffer[5] = keyReportData[4];
			    txBuffer[6] = keyReportData[5];
			    txBuffer[7] = keyReportData[6];
			    txBuffer[8] = keyReportData[7];
				USBD_Write (EP1IN, txBuffer, 9, false);
	    		keyReportNeeded = false;
	    	}
	    }
	}
}

void USBD_DeviceStateChangeCb(USBD_State_TypeDef oldState,
                              USBD_State_TypeDef newState)
{
  // If not configured or in suspend, disable the LED
  if (newState < USBD_STATE_SUSPENDED)
  {
  }
  // Entering suspend mode, power internal and external blocks down
  else if (newState == USBD_STATE_SUSPENDED)
  {
    // Abort any pending transfer
    USBD_AbortTransfer(EP1IN);
  }
  else if (newState == USBD_STATE_CONFIGURED)
  {
	    idleTimerSet(POLL_RATE);
		USBD_Read (EP1OUT, rxBuffer, USBD_READ_SIZE, true);
  }

  // Exiting suspend mode, power internal and external blocks up
  if (oldState == USBD_STATE_SUSPENDED)
  {
  }
}

bool USBD_IsSelfPoweredCb(void)
{
  return true;
}

USB_Status_TypeDef USBD_SetupCmdCb(SI_VARIABLE_SEGMENT_POINTER(
                                     setup,
                                     USB_Setup_TypeDef,
                                     MEM_MODEL_SEG))
{
  USB_Status_TypeDef retVal = USB_STATUS_REQ_UNHANDLED;

  if ((setup->bmRequestType.Type == USB_SETUP_TYPE_STANDARD)
      && (setup->bmRequestType.Direction == USB_SETUP_DIR_IN)
      && (setup->bmRequestType.Recipient == USB_SETUP_RECIPIENT_INTERFACE))
  {
    // A HID device must extend the standard GET_DESCRIPTOR command
    // with support for HID descriptors.
    switch (setup->bRequest)
    {
      case GET_DESCRIPTOR:
        if ((setup->wValue >> 8) == USB_HID_REPORT_DESCRIPTOR)
        {
          switch (setup->wIndex)
          {
            case 0: // Interface 0

              USBD_Write(EP0,
                         (SI_VARIABLE_SEGMENT_POINTER(, uint8_t, SI_SEG_GENERIC))ReportDescriptor0,
                         EFM8_MIN(sizeof(ReportDescriptor0), setup->wLength),
                         false);
              retVal = USB_STATUS_OK;
              break;

            default: // Unhandled Interface
              break;
          }
        }
        else if ((setup->wValue >> 8) == USB_HID_DESCRIPTOR)
        {
          switch (setup->wIndex)
          {
            case 0: // Interface 0

              USBD_Write(EP0,
                         (SI_VARIABLE_SEGMENT_POINTER(, uint8_t, SI_SEG_GENERIC))(&configDesc[18]),
                         EFM8_MIN(USB_HID_DESCSIZE, setup->wLength),
                         false);
              retVal = USB_STATUS_OK;
              break;

            default: // Unhandled Interface
              break;
          }
        }
        break;
    }
  }
  else if ((setup->bmRequestType.Type == USB_SETUP_TYPE_CLASS)
           && (setup->bmRequestType.Recipient == USB_SETUP_RECIPIENT_INTERFACE)
           && (setup->wIndex == HID_VENDOR_IFC))
  {
    // Implement the necessary HID class specific commands.
    switch (setup->bRequest)
    {
      case USB_HID_SET_IDLE:
        if (((setup->wValue & 0xFF) == 0)             // Report ID
            && (setup->wLength == 0)
            && (setup->bmRequestType.Direction != USB_SETUP_DIR_IN))
        {
          idleTimerSet(setup->wValue >> 8);
          retVal = USB_STATUS_OK;
        }
        break;

      case USB_HID_GET_IDLE:
        if ((setup->wValue == 0)                      // Report ID
            && (setup->wLength == 1)
            && (setup->bmRequestType.Direction == USB_SETUP_DIR_IN))
        {
          tmpBuffer = idleGetRate();
          USBD_Write(EP0, &tmpBuffer, 1, false);
          retVal = USB_STATUS_OK;
        }
        break;
    }
  }

  return retVal;
}

uint16_t USBD_XferCompleteCb(uint8_t epAddr,
                             USB_Status_TypeDef status,
                             uint16_t xferred,
                             uint16_t remaining)
{
	int i;

	UNREFERENCED_ARGUMENT(status);
	UNREFERENCED_ARGUMENT(xferred);
	UNREFERENCED_ARGUMENT(remaining);

	if (epAddr == EP1OUT) {
		if (rxBuffer[0] == 0x03) {
			// TODO -- check xferred length
			tallyUpdateNeeded = true;
			for (i = 0; i < 4; i++) {
				tallyUpdateData[i] = rxBuffer[1+i];
			}
		} else if (rxBuffer[0] == 0x04) {
			// TODO -- check xferred length
			backlightUpdateNeeded = true;
			for (i = 0; i < 4; i++) {
				backlightUpdateData[i] = rxBuffer[1+i];
			}
		} else if (rxBuffer[0] == 0x05) {
			// TODO -- check xferred length
			displayUpdateNeeded = true;
			// report ID + address byte + 20 characters
			for (i = 0; i < 22; i++) {
				displayUpdateData[i] = rxBuffer[i];
			}
		} else if (rxBuffer[0] == 0x06) {
			// TODO -- check xferred length
			displayUpdateNeeded = true;
			// report ID + address byte + 32 characters
			for (i = 0; i < 34; i++) {
				displayUpdateData[i] = rxBuffer[i];
			}
		}
		USBD_Read (EP1OUT, rxBuffer, USBD_READ_SIZE, true);
	}

	return 0;
}
