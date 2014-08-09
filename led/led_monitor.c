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

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/time.h>

#include <math.h>

#include <wiringPi.h>
#include <wiringPiSPI.h>

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

#define CHANNEL 0



float stddev;
float mean; 
#define CALIBRATION_SIZE 20
float sensor_calibration[CALIBRATION_SIZE];




float update_stats(float * x, int n, float * out_mean, float * out_stddev) {
    int  i;
    float average, variance, std_deviation, sum = 0, sum1 = 0;
    for (i = 0; i < n; i++) {
        sum = sum + x[i];
    }
    average = sum / (float)n;
    /*  Compute  variance  and standard deviation  */
    for (i = 0; i < n; i++) {
        sum1 = sum1 + pow((x[i] - average), 2);
    }
    variance = sum1 / (float)n;
    std_deviation = MAX(sqrt(variance),3);
    *out_mean=average;
    *out_stddev=std_deviation;
 
    return std_deviation;
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

	wiringPiSPIDataRW (0, spiData, 2) ;

	return ((spiData [0] << 7) | (spiData [1] >> 1)) & 0x3FF ;
}

void calibrate_sensor(int aq) {
	//calibrate the sensor
	if (aq==1) {
		int i=0;
		for (i=0; i<CALIBRATION_SIZE; i++) {
			sensor_calibration[i] = sample(0);
			delay(1);
		}
	}
	update_stats(sensor_calibration,CALIBRATION_SIZE,&mean,&stddev);
	fprintf(stdout, "u/std %f/%f\n", mean, stddev);
}





int main (int argc, char ** argv) {


	wiringPiSetupGpio();
	//lets setup enable pin
	if (wiringPiSPISetup(CHANNEL, 1000000) < 0) {
		fprintf (stderr, "SPI Setup failed: %s\n", strerror (errno));
		exit(errno);
	}



	calibrate_sensor(1);
	fprintf(stderr,"MEAN %f STDDEV %f\n",mean, stddev);	
	return 0;
}



