/*
 * mt3339ctrl.c
 *
 *  Created on: 27.01.2014
 *      Author: dreske
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <termios.h>
#include <fcntl.h>
#include <unistd.h>

static speed_t baudRates[] = {
B0, B50, B75, B110, B134, B150, B200, B300, B600, B1200,
B1800, B2400, B4800, B9600, B19200, B38400, B57600,
B115200, B230400 };

static int baudRatesInt[] = { 0, 50, 75, 110, 134, 150, 200, 300, 600, 1200,
		1800, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 230400 };

int getBaudrate(int baudrate) {
	for (int i = 0; i < 19; i++) {
		if (baudrate == baudRatesInt[i]) {
			return baudRates[i];
		}
	}
	return -1;
}

void printUsage() {
	printf("Usage: mt3339ctrl <packetNum> <dataField> <device> <baudrate>\n");
	printf("Packets:\n");
	printf("\tSet baudrate: 251 <baudrate>\n");
	printf("\tSet update rate: 220 <interval (ms)>\n");
}

unsigned char calculateChecksum(char* action) {
	unsigned char chk = 0;
	int i = 0;
	char c;
	while ((c = action[i]) != 0) {
		chk ^= c;
		i++;
	}
	return chk;
}

int applyAction(int packetNum, char* dataField, int argc, char* argv[]) {
	char* device = argv[argc - 2];
	int baudrate = atoi(argv[argc - 1]);
	
	int fd = open(device, O_RDWR | O_NOCTTY);
	if (fd < 0) {
		fprintf(stderr, "Error: Error opening device: %s\n", strerror(errno));
		return 1;
	}

	if (!isatty(fd)) {
		fprintf(stderr, "Error: %s is not terminal device\n", device);
		return 1;
	}

	struct termios config;
	memset(&config, 0, sizeof(config));
	if (!tcgetattr(fd, &config) < 0) {
		fprintf(stderr, "Error: Error getting terminal attributes: %s\n",
				strerror(errno));
		return 1;
	}
	config.c_iflag = 0;
	config.c_oflag = 0;
	config.c_cflag = CS8 | CREAD | CLOCAL; // 8n1, see termios.h for more information
	config.c_lflag = 0;
	config.c_cc[VMIN] = 1;
	config.c_cc[VTIME] = 5;

	int speed = getBaudrate(baudrate);
	if (speed < 0) {
		fprintf(stderr, "Error: Invalid baudrate: %d\n", baudrate);
		return 1;
	}
	cfsetospeed(&config, speed);
	cfsetispeed(&config, speed);

	if (tcsetattr(fd, TCSANOW, &config) < 0) {
		fprintf(stderr, "Error: Error setting terminal attributes: %s\n",
				strerror(errno));
		return 1;
	}
	
	//prepare action
	char action[255];
	sprintf(action, "PMTK%d,%s", packetNum, dataField);
	
	//pack (preamble, checksum and <CR> <LF>)
	char packet[255];
	sprintf(packet, "$%s*%02x\r\n", action, calculateChecksum(action));
	
	write(fd, packet, strlen(packet));
	close(fd);
	return 0;
}

int main(int argc, char *argv[]) {
	if (argc < 5) {
		printUsage();
		return 1;
	}

	int packetNum = atoi(argv[1]);
	char* dataField = argv[2];
	
	return applyAction(packetNum, dataField, argc, argv);
}

