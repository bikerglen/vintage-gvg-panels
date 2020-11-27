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


//---------------------------------------------------------------------------------------------
// Globals
//

int fd = -1;

const uint8_t loremipsum[160] = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Donec eget tincidunt velit, sit amet lobortis justo. Nullam laoreet quam et accumsan faucibus volutpat.";

// The monster and pumpkin icons are CC BY-SA by Hea Poh Lin and 
// iconcheese from The Noun Project.

const uint8_t pumpkin[128] = {	
	0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x80,0xc0,0xc0,0xe0,0xe0,0xf1,0xe3,0xfe,0xfc,
	0xf8,0xf0,0xe0,0xf0,0xe0,0xe0,0xc0,0xc0,0x80,0x80,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0xe0,0xf8,0xfc,0xff,0xff,0xbf,0x0f,0x1f,0x3f,0x7f,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0x7f,0x3f,0x1f,0x0f,0xbf,0xff,0xff,0xfc,0xf8,0xe0,0x00,0x00,
	0x00,0x3f,0xff,0xff,0xe7,0x0f,0xbf,0x7f,0x7e,0x3c,0x7c,0xfc,0xfc,0x7f,0x7f,0xff,
	0xff,0x7f,0x7f,0xfc,0xfc,0x7c,0x3c,0x7e,0x7f,0xbf,0x0f,0xe7,0xff,0xff,0x3f,0x00,
	0x00,0x00,0x01,0x03,0x0f,0x0f,0x1e,0x3c,0x3c,0x7e,0x7c,0x71,0xf0,0x7c,0xf8,0xf3,
	0xe3,0xf8,0x78,0xf1,0x70,0x7e,0x7c,0x3c,0x3c,0x1e,0x0f,0x0f,0x03,0x01,0x00,0x00
};

const uint8_t monster[128] = {
	0x00,0x00,0x00,0x00,0x00,0x00,0x80,0xe0,0xf0,0xf0,0xf0,0xf8,0xfc,0xfc,0xfc,0xf8,
	0xf8,0xfc,0xfc,0xfc,0xf8,0xf0,0xf0,0xf0,0xe0,0x80,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x98,0xfd,0xff,0xff,0xe7,0x67,0x4f,0x6f,0x4f,0xdf,0xff,0xff,
	0xff,0xff,0xdf,0x6f,0x4f,0x6f,0x67,0xe7,0xff,0xff,0xfd,0x98,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x01,0x05,0x0f,0x0f,0x7f,0x7c,0x38,0x7a,0xfb,0xf8,0x7c,0xff,
	0xff,0x7c,0xf8,0xfb,0x7a,0x38,0x7c,0x7f,0x0f,0x0f,0x05,0x01,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x20,0x30,0x30,0x3f,0x3f,0x00,0x00,
	0x00,0x00,0x3f,0x3f,0x30,0x30,0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
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

	// sequence tally lights
	if (tallyTimer == 0) {
		uint32_t tallyLights = 1 << tallyState;
		buffer[ 0] = 0x03;	// report ID
		buffer[ 1] = tallyLights >> 24;
		buffer[ 2] = tallyLights >> 16;
		buffer[ 3] = tallyLights >>  8;
		buffer[ 4] = tallyLights >>  0;
		write (fd, buffer, 5);
		if (++tallyState >= 32) {
			tallyState = 0;
		}
	}

	// increment tally timer modulo 25
	if (++tallyTimer >= 25) {
		tallyTimer = 0;
	}


	// change the backlight pattern periodically
	if (backlightTimer == 0) {
		switch (backlightState) {
			 case 0: // amber
				buffer[ 0] = 0x04;	// report ID
				buffer[ 1] = 0xff;
				buffer[ 2] = 0xff;
				buffer[ 3] = 0xff;
				buffer[ 4] = 0xff;
				write (fd, buffer, 5);

				for (i = 0; i < 8; i++) {
					buffer[ 0] = 0x05;	// report ID
					buffer[ 1] = i*4;
					for (int r = 0; r < 4; r++) {
						for (int c = 0; c < 5; c++) {
							buffer[2+5*r+c] = loremipsum[5*r + 20*i + c];
						}
					}
					write (fd, buffer, 22);
				}
				break;
			 case 1: // red
				buffer[ 0] = 0x04;	// report ID
				buffer[ 1] = 0xff;
				buffer[ 2] = 0xff;
				buffer[ 3] = 0x00;
				buffer[ 4] = 0x00;
				write (fd, buffer, 5);

				memset (buffer, 0, 32);
				buffer[ 0] = 0x05;	// report ID
				buffer[ 1] = 0*4 + 3;
				buffer[ 2] = BIG_EXCLAM;
				write (fd, buffer, 22);
				buffer[ 1] = 1*4 + 3;
				buffer[ 2] = BIG_D;
				write (fd, buffer, 22);
				buffer[ 1] = 2*4 + 3;
				buffer[ 2] = BIG_A;
				write (fd, buffer, 22);
				buffer[ 1] = 3*4 + 3;
				buffer[ 2] = BIG_N;
				write (fd, buffer, 22);
				buffer[ 1] = 4*4 + 3;
				buffer[ 2] = BIG_G;
				write (fd, buffer, 22);
				buffer[ 1] = 5*4 + 3;
				buffer[ 2] = BIG_E;
				write (fd, buffer, 22);
				buffer[ 1] = 6*4 + 3;
				buffer[ 2] = BIG_R;
				write (fd, buffer, 22);
				buffer[ 1] = 7*4 + 3;
				buffer[ 2] = BIG_EXCLAM;
				write (fd, buffer, 22);
				break;

			 case 2: // green
				buffer[ 0] = 0x04;	// report ID
				buffer[ 1] = 0x00;
				buffer[ 2] = 0x00;
				buffer[ 3] = 0xff;
				buffer[ 4] = 0xff;
				write (fd, buffer, 5);

				memset (buffer, 0, 32);
				buffer[ 0] = 0x05;	// report ID
				buffer[ 1] = 0*4 + 3;
				buffer[ 2] = BIG_A;
				write (fd, buffer, 22);
				buffer[ 1] = 1*4 + 3;
				buffer[ 2] = BIG_L;
				write (fd, buffer, 22);
				buffer[ 1] = 2*4 + 3;
				buffer[ 2] = BIG_L;
				write (fd, buffer, 22);
				buffer[ 1] = 3*4 + 3;
				buffer[ 2] = BIG_SPACE;
				write (fd, buffer, 22);
				buffer[ 1] = 4*4 + 3;
				buffer[ 2] = BIG_C;
				write (fd, buffer, 22);
				buffer[ 1] = 5*4 + 3;
				buffer[ 2] = BIG_O;
				write (fd, buffer, 22);
				buffer[ 1] = 6*4 + 3;
				buffer[ 2] = BIG_O;
				write (fd, buffer, 22);
				buffer[ 1] = 7*4 + 3;
				buffer[ 2] = BIG_L;
				write (fd, buffer, 22);
				break;

			 case 3: // red green checkerboard
				buffer[ 0] = 0x04;	// report ID
				buffer[ 1] = 0xaa;
				buffer[ 2] = 0x55;
				buffer[ 3] = 0x55;
				buffer[ 4] = 0xaa;
				write (fd, buffer, 5);

				for (i = 0; i < 8; i++) {
					buffer[ 0] = 0x05;	// report ID
					buffer[ 1] = i*4 + 2;
					buffer[ 2] = 'A';
					buffer[ 3] = 'B';
					buffer[ 4] = 'C';
					buffer[ 5] = 'D';
					buffer[ 6] = 'E';
					buffer[ 7] = 'F';
					buffer[ 8] = ' ';
					buffer[ 9] = ' ';
					buffer[10] = ' ';
					buffer[11] = ' ';
					buffer[12] = ' ';
					buffer[13] = ' ';
					buffer[14] = ' ';
					buffer[15] = ' ';
					buffer[16] = ' ';
					buffer[17] = ' ';
					buffer[18] = ' ';
					buffer[19] = ' ';
					buffer[20] = ' ';
					buffer[21] = ' ';
					write (fd, buffer, 22);
				}
				break;

			 case 4: // amber green alternate
				buffer[ 0] = 0x04;	// report ID
				buffer[ 1] = 0xaa;
				buffer[ 2] = 0xaa;
				buffer[ 3] = 0xff;
				buffer[ 4] = 0xff;
				write (fd, buffer, 5);

				memset (buffer, 0, 32);
				buffer[ 0] = 0x05;	// report ID
				buffer[ 1] = 0*4 + 3;
				buffer[ 2] = BIG_A;
				write (fd, buffer, 22);
				buffer[ 1] = 1*4 + 3;
				buffer[ 2] = BIG_W;
				write (fd, buffer, 22);
				buffer[ 1] = 2*4 + 3;
				buffer[ 2] = BIG_E;
				write (fd, buffer, 22);
				buffer[ 1] = 3*4 + 3;
				buffer[ 2] = BIG_S;
				write (fd, buffer, 22);
				buffer[ 1] = 4*4 + 3;
				buffer[ 2] = BIG_O;
				write (fd, buffer, 22);
				buffer[ 1] = 5*4 + 3;
				buffer[ 2] = BIG_M;
				write (fd, buffer, 22);
				buffer[ 1] = 6*4 + 3;
				buffer[ 2] = BIG_E;
				write (fd, buffer, 22);
				buffer[ 1] = 7*4 + 3;
				buffer[ 2] = BIG_SPACE;
				write (fd, buffer, 22);
				break;

			 case 5: // green amber alternate
				buffer[ 0] = 0x04;	// report ID
				buffer[ 1] = 0x55;
				buffer[ 2] = 0x55;
				buffer[ 3] = 0xff;
				buffer[ 4] = 0xff;
				write (fd, buffer, 5);

				memset (buffer, 0, 32);
				buffer[ 0] = 0x05;	// report ID
				buffer[ 1] = 0*4 + 3;
				buffer[ 2] = BIG_SPACE;
				write (fd, buffer, 22);
				buffer[ 1] = 1*4 + 3;
				buffer[ 2] = BIG_SPACE;
				write (fd, buffer, 22);
				buffer[ 1] = 2*4 + 3;
				buffer[ 2] = BIG_S;
				write (fd, buffer, 22);
				buffer[ 1] = 3*4 + 3;
				buffer[ 2] = BIG_A;
				write (fd, buffer, 22);
				buffer[ 1] = 4*4 + 3;
				buffer[ 2] = BIG_U;
				write (fd, buffer, 22);
				buffer[ 1] = 5*4 + 3;
				buffer[ 2] = BIG_C;
				write (fd, buffer, 22);
				buffer[ 1] = 6*4 + 3;
				buffer[ 2] = BIG_E;
				write (fd, buffer, 22);
				buffer[ 1] = 7*4 + 3;
				buffer[ 2] = BIG_EXCLAM;
				write (fd, buffer, 22);
				break;

			 case 6: // amber
				buffer[ 0] = 0x04;	// report ID
				buffer[ 1] = 0xff;
				buffer[ 2] = 0xff;
				buffer[ 3] = 0xff;
				buffer[ 4] = 0xff;
				write (fd, buffer, 5);

				for (i = 0; i < 8; i++) {
					if ((i & 1) == 0) {
						buffer[ 0] = 0x06; // report ID
						buffer[ 1] = 4*i + 0;
						memcpy (&buffer[2], &pumpkin[0], 32);
						write (fd, buffer, 34);
						buffer[ 0] = 0x06; // report ID
						buffer[ 1] = 4*i + 1;
						memcpy (&buffer[2], &pumpkin[32], 32);
						write (fd, buffer, 34);
						buffer[ 0] = 0x06; // report ID
						buffer[ 1] = 4*i + 2;
						memcpy (&buffer[2], &pumpkin[64], 32);
						write (fd, buffer, 34);
						buffer[ 0] = 0x06; // report ID
						buffer[ 1] = 4*i + 3;
						memcpy (&buffer[2], &pumpkin[96], 32);
						write (fd, buffer, 34);
					} else {
						buffer[ 0] = 0x06; // report ID
						buffer[ 1] = 4*i + 0;
						memcpy (&buffer[2], &monster[0], 32);
						write (fd, buffer, 34);
						buffer[ 0] = 0x06; // report ID
						buffer[ 1] = 4*i + 1;
						memcpy (&buffer[2], &monster[32], 32);
						write (fd, buffer, 34);
						buffer[ 0] = 0x06; // report ID
						buffer[ 1] = 4*i + 2;
						memcpy (&buffer[2], &monster[64], 32);
						write (fd, buffer, 34);
						buffer[ 0] = 0x06; // report ID
						buffer[ 1] = 4*i + 3;
						memcpy (&buffer[2], &monster[96], 32);
						write (fd, buffer, 34);
					}
				}
				break;

			 case 7: // green
				buffer[ 0] = 0x04;	// report ID
				buffer[ 1] = 0x00;
				buffer[ 2] = 0x00;
				buffer[ 3] = 0xff;
				buffer[ 4] = 0xff;
				write (fd, buffer, 5);

				for (i = 0; i < 8; i++) {
					buffer[ 0] = 0x05;	// report ID
					buffer[ 1] = i*4 + 1;
					buffer[ 2] = 'A';
					buffer[ 3] = 'B';
					buffer[ 4] = 'C';
					buffer[ 5] = 'D';
					buffer[ 6] = 'E';
					buffer[ 7] = 'F';
					buffer[ 8] = 'G';
					buffer[ 9] = 'H';
					buffer[10] = 'I';
					buffer[11] = 'J';
					buffer[12] = 'K';
					buffer[13] = 'L';
					buffer[14] = 'M';
					buffer[15] = 'N';
					buffer[16] = 'O';
					buffer[17] = 'P';
					buffer[18] = ' ';
					buffer[19] = ' ';
					buffer[20] = ' ';
					buffer[21] = ' ';
					write (fd, buffer, 22);
				}
				break;
		}
		if (++backlightState >= 8) {
			backlightState = 0;
		}
	}

	// increment backlight timer modulo 150
	if (++backlightTimer >= 150) {
		backlightTimer = 0;
	}

}
