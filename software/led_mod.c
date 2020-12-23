// Optional LED mod
// Thread that reads the rotation angle of the plater and writes a RPC call to serial port (UART3)

#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <unistd.h>
#include <sys/time.h>
#include "player.h"
#include "xwax.h"


// file descriptor for UART3 serial port
int uart3_fd;



void LED_Mod_init()
{
	printf("\nInitializing LED mod ..\n");

	// find the TTY name of uart3
	FILE *fp;
	char serial3_name[32];
	// 0x1c28c00 is the UART3/PG9 (see DTB/DTS file)
	fp = popen("dmesg | grep \"0x1c28c00\" | grep -Eow \"ttyS[0-9]+\"", "r");
	if (fp == NULL) {
		printf("[LED mod] Failed to determine uart3 tty name.\n");
		exit(1);
	}
	fgets(serial3_name, sizeof(serial3_name), fp);
	pclose(fp);
	serial3_name[strcspn(serial3_name, "\n")] = 0; // trim newline
	char serial3_name_full[48] = "/dev/";
	strncat(serial3_name_full, serial3_name, sizeof(serial3_name));
	
	// Initialize UART3 (for LED mod)
	printf("Connecting LED mod to serial interface: %s\n", serial3_name_full);
	uart3_fd = open(serial3_name_full, O_RDWR);
	if (uart3_fd < 0) {
		printf("[LED mod] Error %i from open: %s\n", errno, strerror(errno));
		exit(errno);
	}
	struct termios tty_ledmod;
	if (tcgetattr(uart3_fd, &tty_ledmod) != 0) {
 	    printf("[LED mod] Error %i from tcgetattr: %s\n", errno, strerror(errno));
		exit(errno);
	}

	// https://blog.mbedded.ninja/programming/operating-systems/linux/linux-serial-ports-using-c-cpp/#basic-setup-in-c

	tty_ledmod.c_cflag &= ~PARENB; // Clear parity bit, disabling parity
	tty_ledmod.c_cflag &= ~CSTOPB; // Clear stop field, only one stop bit used in communication
	tty_ledmod.c_cflag |= CS8;	   // 8 bits per byte

	// output config
	tty_ledmod.c_oflag &= ~OPOST; // Prevent special interpretation of output bytes (e.g. newline chars)
	tty_ledmod.c_oflag &= ~ONLCR; // Prevent conversion of newline to carriage return/line feed

	// Set in/out baud rate to be 115200
	cfsetispeed(&tty_ledmod, B115200);
	cfsetospeed(&tty_ledmod, B115200);

	// Save tty settings, also checking for error
	if (tcsetattr(uart3_fd, TCSANOW, &tty_ledmod) != 0) {
		printf("[LED mod] Error %i from tcsetattr: %s\n", errno, strerror(errno));
		exit(errno);
	}
}


void *LED_Mod_InputThread_Simple(void *ptr)
{
	LED_Mod_init();
		
	struct timespec ts_sleep;
	int msec = 20;
	ts_sleep.tv_sec = 0;
	ts_sleep.tv_nsec = (msec % 1000) * 1000000;

	const int rot_msg_max_size = 10;
	char rot_msg[rot_msg_max_size];

	int rotation_angle;
	int size;

	while (1) {
		rotation_angle = (int)player_get_position_angle(&deck[1].player);
		size = snprintf(rot_msg, rot_msg_max_size, "r(%d)\r\n", rotation_angle);		
		//printf("\nWriting rotation to serial: %s\n", rot_msg);

		/* The format of the output is a RPC in the format: r(120)\r\n
		 * 
		 * The receiving side interprets this as direct call to it's r() method, 
		 * passing the angle in degrees as integer.
		 */
		write(uart3_fd, rot_msg, size);
		
		tcdrain(uart3_fd);
		nanosleep(&ts_sleep, NULL);
	}	
}


// Start the thread for the LED mod
void LED_Mod_Start()
{
	pthread_t thread1;
	const char *message1 = "Thread LED mod";
	int iret1;

	iret1 = pthread_create(&thread1, NULL, LED_Mod_InputThread_Simple, (void *)message1);
	if (iret1) {
		fprintf(stderr, "Error - pthread_create() for LED mod return code: %d\n", iret1);
		exit(EXIT_FAILURE);
	}
}

