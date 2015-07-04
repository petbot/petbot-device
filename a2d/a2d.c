/*#This file is part of PetBot.
#
#    PetBot is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation, either version 3 of the License, or
#    (at your option) any later version.
#
#    PetBot is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with Foobar.  If not, see <http://www.gnu.org/licenses/>.*/
#include <sys/ioctl.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/time.h>
#include <linux/spi/spidev.h>
#include <math.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))



static uint32_t speed = 1000000;
static int spi_fd = 0;

static char       *spiDev0 = "/dev/spidev0.0" ;
static char       *spiDev1 = "/dev/spidev0.1" ;

static uint8_t     spiMode   = 0 ;
static uint8_t     spiBPW    = 8 ;
static uint16_t    spiDelay  = 0;

float stddev;
float mean; 

int setupSPI (int speed) {

  if ((spi_fd = open (spiDev0, O_RDWR)) < 0)
    return -1 ;

  fprintf(stderr,"Opened for SPI on fd %d\n",spi_fd);
  if (ioctl (spi_fd, SPI_IOC_WR_MODE, &spiMode)         < 0) return -1 ;
  if (ioctl (spi_fd, SPI_IOC_RD_MODE, &spiMode)         < 0) return -1 ;

  if (ioctl (spi_fd, SPI_IOC_WR_BITS_PER_WORD, &spiBPW) < 0) return -1 ;
  if (ioctl (spi_fd, SPI_IOC_RD_BITS_PER_WORD, &spiBPW) < 0) return -1 ;

  if (ioctl (spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed)   < 0) return -1 ;
  if (ioctl (spi_fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed)   < 0) return -1 ;

  return spi_fd ;
}


int RWSPI (int channel, unsigned char *data, int len) {
  struct spi_ioc_transfer spi ;

  channel &= 1 ;

  spi.tx_buf        = (unsigned long)data ;
  spi.rx_buf        = (unsigned long)data ;
  spi.len           = len ;
  spi.delay_usecs   = spiDelay ;
  spi.speed_hz      = speed ;
  spi.bits_per_word = spiBPW ;

  return ioctl (spi_fd, SPI_IOC_MESSAGE(1), &spi) ;
}



unsigned int sample(int channel) {
	uint8_t spiData [2] ;

	uint8_t chanBits ;

	if (channel == 0)
		chanBits = 0b11010000 ;
	else
		chanBits = 0b11110000 ;

	spiData [0] = chanBits ;
	spiData [1] = 0 ;

	RWSPI (0, spiData, 2) ;

	return ((spiData [0] << 7) | (spiData [1] >> 1)) & 0x3FF ;
}

void calibrate_sensor(int aq) {
	//calibrate the sensor
        int i;
	for (i=0; 1==1; i++) {
		unsigned int a  = sample(0);
		unsigned int b  = sample(1);
		fprintf(stdout, "%u %u\n", a, b);
	}
}



int main (int argc, char ** argv) {


	//wiringPiSetupGpio();
	//lets setup enable pin
	if (setupSPI( 1000000) < 0) {
		fprintf (stderr, "SPI Setup failed: %s\n", strerror (errno));
		exit(errno);
	}

	calibrate_sensor(1);
	return 0;
}



