//// Programa para controlar el sensor DS1621
////
//// Este programa configurara el sensor DS1621 de forma
//// que se le lea la temperatura cada segundo.
////
//// Manuel Milla 2014

#include "DS1621.h"

int fd  = -1;

int ds1621_setup(){												// File descrition
	char *fileName = "/dev/i2c-1";								// Name of the port we will be using
	int  address = 0x48;										// Address of the SRF02 shifted right one bit
	unsigned char buf[10];										// Buffer for data being read/ written on the i2c bus

	if ((fd = open(fileName, O_RDWR)) < 0) {					// Open port for reading and writing
		printMsg(stderr, DS1621, "Failed to open i2c port\n");
		return EXIT_FAILURE;
	}

	if (ioctl(fd, I2C_SLAVE, address) < 0) {					// Set the port options and set the address of the device we wish to speak to
		printMsg(stderr, DS1621, "Unable to get bus access to talk to slave\n");
		return EXIT_FAILURE;
	}


	buf[0]= 0xee; // start convert
	if ((write(fd, buf, 1)) != 1) {
		printMsg(stderr, DS1621, "Unable to write\n");
		return EXIT_FAILURE;
	}
	buf[0]= 0xac; // Access config
	buf[1]= 0x00;

	if ((write(fd, buf, 2)) != 2) {
		printMsg(stderr, DS1621, "Unable to wrie\n");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;

}

int readDS1621Sensor(signed char* highByte, unsigned char* lowByte)
{
	//int fd;
	unsigned char buf[10];
	// Commands for performing a ranging
	buf[0] =0xaa;

	if ((write(fd, buf, 1)) != 1) {								// Write commands to the i2c port
		printMsg(stderr, DS1621, "Error writing to i2c slave\n");
		return EXIT_FAILURE;
	}
	// This sleep waits for the ping to come back

	if (read(fd, buf, 2) != 2) {								// Read back data into buf[]
		printMsg(stderr, DS1621, "Unable to read from slave\n");
		return EXIT_FAILURE;
	}
	else {
		*highByte = buf[0];
		*lowByte = buf[1];
	}

	return EXIT_SUCCESS;
}
