//---------------------------------------------------------------------------------------------
// Includes
//

#include <linux/types.h>
#include <linux/input.h>
#include <linux/hidraw.h>

#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>


//---------------------------------------------------------------------------------------------
// Defines
//


//---------------------------------------------------------------------------------------------
// Typedefs
//


//---------------------------------------------------------------------------------------------
// Prototypes
//

const char *bus_str (int bus);
void Quit (int sig);
void timer_handler (int signum);


//---------------------------------------------------------------------------------------------
// Globals
//

int fd = -1;

// each 32 word corresponds to tally lights 31...0
// this is left to right the 18 tally lights on the machine control panel
const uint32_t tallyLights[18] = {
	0x00000400, 0x00000800, 0x00001000, 0x00002000, // 10, 11, 12, 13
	0x00004000, 0x00008000, 0x00010000, 0x00020000, // 14, 15, 16, 17
	0x00040000, 0x00080000, 0x01000000, 0x02000000, // 18, 19, 24, 25
	0x04000000, 0x80000000, 0x20000000, 0x08000000, // 26, 31, 29, 27
	0x40000000, 0x10000000                          // 30, 28
};


//---------------------------------------------------------------------------------------------
// Main
//

int main(int argc, char **argv)
{
	uint8_t buf[256];
	int i, j, n, res, desc_size = 0;
	struct hidraw_report_descriptor rpt_desc;
	struct hidraw_devinfo info;
	char *device = "/dev/hidraw0";

    struct sigaction sa;
    struct itimerval timer;

    // trap ctrl-c to call quit function
    signal (SIGINT, Quit);

	if (argc > 1)
		device = argv[1];

	/* Open the Device with non-blocking reads. In real life,
	   don't use a hard coded path; use libudev instead. */
	fd = open(device, O_RDWR|O_NONBLOCK);

	if (fd < 0) {
		perror("Unable to open device");
		return 1;
	}

	memset(&rpt_desc, 0x0, sizeof(rpt_desc));
	memset(&info, 0x0, sizeof(info));
	memset(buf, 0x0, sizeof(buf));

	/* Get Report Descriptor Size */
	res = ioctl(fd, HIDIOCGRDESCSIZE, &desc_size);
	if (res < 0)
		perror("HIDIOCGRDESCSIZE");
	else
		printf("Report Descriptor Size: %d\n", desc_size);

	/* Get Report Descriptor */
	rpt_desc.size = desc_size;
	res = ioctl(fd, HIDIOCGRDESC, &rpt_desc);
	if (res < 0) {
		perror("HIDIOCGRDESC");
	} else {
		printf("Report Descriptor:\n");
		for (i = 0; i < rpt_desc.size; i++)
			printf("%hhx ", rpt_desc.value[i]);
		puts("\n");
	}

	/* Get Raw Name */
	res = ioctl(fd, HIDIOCGRAWNAME(256), buf);
	if (res < 0)
		perror("HIDIOCGRAWNAME");
	else
		printf("Raw Name: %s\n", buf);

	/* Get Physical Location */
	res = ioctl(fd, HIDIOCGRAWPHYS(256), buf);
	if (res < 0)
		perror("HIDIOCGRAWPHYS");
	else
		printf("Raw Phys: %s\n", buf);

	/* Get Raw Info */
	res = ioctl(fd, HIDIOCGRAWINFO, &info);
	if (res < 0) {
		perror("HIDIOCGRAWINFO");
	} else {
		printf("Raw Info:\n");
		printf("\tbustype: %d (%s)\n",
			info.bustype, bus_str(info.bustype));
		printf("\tvendor: 0x%04hx\n", info.vendor);
		printf("\tproduct: 0x%04hx\n", info.product);
	}

    // install timer handler
    memset (&sa, 0, sizeof (sa));
    sa.sa_handler = &timer_handler;
    sigaction (SIGALRM, &sa, NULL);

    // configure the timer to expire after 10 msec
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = 10000;

    // and every 10 msec after that.
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 10000;

    // start the timer
    setitimer (ITIMER_REAL, &timer, NULL);

    // wait forever
    while (1) {
		sleep (1);
    }

	while (1) {
		for (i = 0; i < 10; i++) { 
/*
			buf[0] = 0x01;
			buf[1] = leds[i] >> 8;
			buf[2] = leds[i];

			res = write(fd, buf, 3);
			if (res < 0) {
				printf("Error: %d\n", errno);
				perror("write");
			}
*/

			for (n = 0; n < 5; n++) {
				usleep (50000);
				res = read(fd, buf, 17);
				if (res < 0) {
					// perror("read");
				} else if (res > 0) {
					//printf("read() read %d bytes:\n\t", res);
					for (j = 0; j < res; j++) {
						printf("%02x ", buf[j] & 0xff);
					}
					printf("\n");
				}
			}

		}
	}

	close(fd);

	return 0;
}


//---------------------------------------------------------------------------------------------
// Quit
//

void Quit (int sig)
{
	// print bye message
	printf ("Bye!\n");

	// close any open file descriptors
	if (fd >= 0) {
		close (fd);
	}

    // exit
    exit (0);
}


//---------------------------------------------------------------------------------------------
// bus_str
//

const char *bus_str (int bus)
{
	switch (bus) {
		case BUS_USB:
			return "USB";
			break;
		case BUS_HIL:
			return "HIL";
			break;
		case BUS_BLUETOOTH:
			return "Bluetooth";
			break;
		case BUS_VIRTUAL:
			return "Virtual";
			break;
		default:
			return "Other";
			break;
	}
}


//---------------------------------------------------------------------------------------------
// timer_handler
//

void timer_handler (int signum)
{
	int i;
    int count;
	uint8_t buffer[256];

	static int messageTimer = 0;
	static int tallyTimer = 0;
	static int tallyState = 0;

    // printf (".");
    // fflush (stdout);

	// check for a packet with keypress data from the board
	count = read(fd, buffer, 17);
	if (count > 0) {
		for (i = 0; i < count; i++) {
			printf("%02x ", buffer[i] & 0xff);
		}
		printf("\n");
	}

	// cycle through messages
	if (messageTimer == 0) {
		buffer[ 0] = 0x02;	// report ID
		buffer[ 1] = 'H';
		buffer[ 2] = 'e';
		buffer[ 3] = 'l';
		buffer[ 4] = 'l';
		buffer[ 5] = 'o';
		buffer[ 6] = ' ';
		buffer[ 7] = 'w';
		buffer[ 8] = 'o';
		buffer[ 9] = 'r';
		buffer[10] = 'l';
		buffer[11] = 'd';
		buffer[12] = '!';
		write (fd, buffer, 13);
	} else if (messageTimer == 50) {
		buffer[ 0] = 0x02;	// report ID
		buffer[ 1] = 'A';
		buffer[ 2] = 'B';
		buffer[ 3] = 'C';
		buffer[ 4] = 'D';
		buffer[ 5] = 'E';
		buffer[ 6] = 'F';
		buffer[ 7] = '6';
		buffer[ 8] = 'H';
		buffer[ 9] = 'I';
		buffer[10] = 'J';
		buffer[11] = 'K';
		buffer[12] = 'L';
		write (fd, buffer, 13);
	} else if (messageTimer == 100) {
		buffer[ 0] = 0x02;	// report ID
		buffer[ 1] = ' ';
		buffer[ 2] = '0';
		buffer[ 3] = '1';
		buffer[ 4] = '2';
		buffer[ 5] = '3';
		buffer[ 6] = '4';
		buffer[ 7] = '5';
		buffer[ 8] = '6';
		buffer[ 9] = '7';
		buffer[10] = '8';
		buffer[11] = '9';
		buffer[12] = ' ';
		write (fd, buffer, 13);
	} else if (messageTimer == 150) {
		buffer[ 0] = 0x02;	// report ID
		buffer[ 1] = 'a';
		buffer[ 2] = 'b';
		buffer[ 3] = 'c';
		buffer[ 4] = 'd';
		buffer[ 5] = 'e';
		buffer[ 6] = 'f';
		buffer[ 7] = '6';
		buffer[ 8] = 'h';
		buffer[ 9] = 'i';
		buffer[10] = 'j';
		buffer[11] = 'k';
		buffer[12] = 'l';
		write (fd, buffer, 13);
	}

	// increment message timer modulo 200
	if (++messageTimer >= 200) {
		messageTimer = 0;
	}
	
	// sequence tally lights
	if (tallyTimer == 0) {
		buffer[ 0] = 0x03;	// report ID
		buffer[ 1] = tallyLights[tallyState] >> 24;
		buffer[ 2] = tallyLights[tallyState] >> 16;
		buffer[ 3] = tallyLights[tallyState] >>  8;
		buffer[ 4] = tallyLights[tallyState] >>  0;
		write (fd, buffer, 5);
		if (++tallyState >= 18) {
			tallyState = 0;
		}
	}

	// increment tally timer modulo 25
	if (++tallyTimer >= 25) {
		tallyTimer = 0;
	}


}
