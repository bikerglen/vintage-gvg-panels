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

#define BIG_A 0
#define BIG_B 1
#define BIG_C 2
#define BIG_D 3
#define BIG_E 4
#define BIG_F 5
#define BIG_G 6
#define BIG_H 7
#define BIG_I 8
#define BIG_J 9
#define BIG_K 10
#define BIG_L 11
#define BIG_M 12
#define BIG_N 13
#define BIG_O 14
#define BIG_P 15
#define BIG_Q 16
#define BIG_R 17
#define BIG_S 18
#define BIG_T 19
#define BIG_U 20
#define BIG_V 21
#define BIG_W 22
#define BIG_X 23
#define BIG_Y 24
#define BIG_Z 25
#define BIG_0 26
#define BIG_1 27
#define BIG_2 28
#define BIG_3 29
#define BIG_4 30
#define BIG_5 31
#define BIG_6 32
#define BIG_7 33
#define BIG_8 34
#define BIG_9 35
#define BIG_EXCLAM 36
#define BIG_SPACE 37


//---------------------------------------------------------------------------------------------
// Typedefs
//


//---------------------------------------------------------------------------------------------
// Prototypes
//

const char *bus_str (int bus);
void Quit (int sig);
void timer_handler (int signum);
uint16_t lfsr (void);


//---------------------------------------------------------------------------------------------
// Globals
//

int fd = -1;
uint16_t lfsr_state = 0x0001;
uint8_t lamp_timers[32];
uint8_t lamp_states[32];


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

	// clear lamp timers and states
	for (i = 0; i <32; i++) {
 		lamp_timers[i] = 0;
		lamp_states[i] = 0;
	}

	// blank display
	memset (buf, 0, 32);
	buf[ 0] = 0x05;  // report ID
	buf[ 1] = 0*4 + 3;
	buf[ 2] = BIG_SPACE;
	write (fd, buf, 22);
	buf[ 1] = 1*4 + 3;
	write (fd, buf, 22);
	buf[ 1] = 2*4 + 3;
	write (fd, buf, 22);
	buf[ 1] = 3*4 + 3;
	write (fd, buf, 22);
	buf[ 1] = 4*4 + 3;
	write (fd, buf, 22);
	buf[ 1] = 5*4 + 3;
	write (fd, buf, 22);
	buf[ 1] = 6*4 + 3;
	write (fd, buf, 22);
	buf[ 1] = 7*4 + 3;
    write (fd, buf, 22);

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

	static int tallyTimer = 0;
	static int tallyState = 0;

	static int backlightTimer = 0;
	static int backlightState = 0;

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

	// "randomly" blink lamps
	for (i = 0; i < 32; i++) {
		if (lamp_timers[i] == 0) {
			uint8_t r = lfsr () % 160; // was 80 // was 40
			uint8_t t = lfsr () % 50; // was 25
			if (r == 0) {
				lamp_timers[i] = 40 + t; // was 20 + t
				lamp_states[i] = 1;
			}
		} else if (lamp_timers[i] < 10) { // was 5
			lamp_states[i] = 0;
		}
		if (lamp_timers[i] != 0) {
			lamp_timers[i]--;
		}
	}

	// update lamps
	buffer[ 0] = 0x03;  // report ID
	buffer[ 1] = 0x00;
	buffer[ 2] = 0x00;
	buffer[ 3] = 0x00;
	buffer[ 4] = 0x00;
	for (i = 0 ; i < 32; i++) {
		if (lamp_states[i]) {
			buffer[4 - (i >> 3)] |= 0x1 << (i & 7);
		}
	}
	write (fd, buffer, 5);
}


uint16_t lfsr (void)
{
    uint16_t newbit;

    newbit = ((lfsr_state >> 0) ^ (lfsr_state >> 2) ^ (lfsr_state >> 3) ^ (lfsr_state >> 5)) /* & 1u */;
    lfsr_state = (lfsr_state >> 1) | (newbit << 15);

    return lfsr_state;
}
