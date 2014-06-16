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

#define M1PIN	27
#define M2PIN	7
#define ENPIN	22

#define TURN_BACK 50

uint8_t buf[2];
char spos[] = "7=0.063\n";
char epos[] = "7=0.21\n";
char stop[] = "7=0\n";

long start_time=0;

float stddev;
float mean; 
#define CALIBRATION_SIZE 20
float sensor_calibration[CALIBRATION_SIZE];


#define RES	50

long gettime() {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	//return (tv.tv_sec) * 1000 + (tv.tv_usec) / 1000 ;
	return tv.tv_sec;
}


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

void cookieInterrupt(void) { 
	//Got a cookie!
	printf("got a cookie!\n");
	//setpos(stop);
	exit(1);
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

int state=0;
int jam2=0;

void turn_back() {
	printf("Turning back\n");
	digitalWrite(ENPIN,0);	
	digitalWrite(M1PIN,1-state);
	digitalWrite(M2PIN,state);
	digitalWrite(ENPIN,1);	
	delay(TURN_BACK);
	digitalWrite(ENPIN,0);	
	printf("Done turning back\n");
}


void move() {
	fprintf(stdout,"move() %d\n",state);
	//change state and start spining
	digitalWrite(ENPIN,0);	
	digitalWrite(M1PIN,state);
	delay(RES);
	digitalWrite(M2PIN,1-state);
	digitalWrite(ENPIN,1);	
	
	printf("spin up\n");

	int i;
	for (i=0; i<6*RES; i++) {
		unsigned int current = sample(1);
		int adj=5*jam2;
		if ( (i>15 && current<(560-adj)) || (i>50 && current <(620-adj)) || (i>80 && current <(700-adj)) || (i>100 && current <(805-adj)) ||  (i>150 && current<(840-adj)) || (i>200 && current<(850-adj))) {
			digitalWrite(ENPIN,0);	
			turn_back();
			printf("sample=%d %d\n", current, i); //get the read from the motor
			printf("jam2\n");
			jam2+=1;
			int j;
			for (j=0; j<20*RES; j++) {
				unsigned int sensor2 = sample(0);
				if (abs(sensor2-mean)>6*stddev) {
					printf("COOKIE DROP!\n");
					digitalWrite(ENPIN,0);	
					exit(1);
				}
			}
			state=1-state;
			return;
		}
		delay(1);
			unsigned int sensor2 = sample(0);
			if (abs(sensor2-mean)>6*stddev) {
				digitalWrite(ENPIN,0);	
				turn_back();
				printf("COOKIE DROP!\n");
				exit(1);
			}
	}
	printf("spun up\n");

	//unsigned int prev=0x4FF;
	//int init_back_down=3;
	int init_back_down=10;
	int back_down=init_back_down;
	for (i=0; i<1000; i++) {
		delay(1);
		unsigned int current = sample(1);
		unsigned int sensor = sample(0);
		if (abs(sensor-mean)>6*stddev) {
			unsigned int sensor2 = sample(0);
			if (abs(sensor2-mean)>6*stddev) {
				digitalWrite(ENPIN,0);	
				turn_back();
				printf("sample=%d vs sensor=%d\n", current, sensor); //get the read from the motor
				printf("COOKIE DROP!\n");
				exit(1);
			}
			
		}	
		//unsigned int mx=MAX(current,prev);
		//unsigned int mn=MIN(current,prev);
		//if (mx-mn<50) {
		if (current<890) { //12v
		//if (current<560) { //1.8+1.5ohm
		//if (current<770) { //1.8ohm
		//if (current<770) { //1.5ohm
		//if (current<610) { //10 ohm
			back_down-=1;
			if (current<865) { //12v 
				printf("sample=%d vs sensor=%d\n", current, sensor); //get the read from the motor
			//if (current<545) { //1.8+1.5ohm
			//if (current<755) { //1.8ohm
			//if (current<755) { //1.5ohm
			//if (current<600) { //10ohm
				back_down-=10;
			}
			if (back_down<=0) {
				digitalWrite(ENPIN,0);	
				turn_back();
				printf("sample=%d vs sensor=%d\n", current, sensor); //get the read from the motor
				printf("jam\n");
				int j;
				for (j=0; j<8*RES; j++) {
					unsigned int sensor2 = sample(0);
					if (abs(sensor2-mean)>6*stddev) {
						digitalWrite(ENPIN,0);	
						printf("COOKIE DROP!\n");
						exit(1);
					}
				}
				state=1-state;
				return;
			}
		} else {
			back_down=init_back_down;
		}
		//prev=current;
	}
	turn_back();
}



int main (int argc, char ** argv) {

	if (argc!=2) {
		printf("%s timeout\n",argv[0]);
		exit(1);
	}

	long timeout = atol(argv[1]);

	if (timeout==0 || timeout>10) {
		printf("timeout must be in range [1,10]\n");
		exit(1);
	}	

	wiringPiSetupGpio();
	//lets setup enable pin
	pinMode(M1PIN,OUTPUT);
	pinMode(M2PIN,OUTPUT);
	pinMode(ENPIN,OUTPUT);
	digitalWrite(ENPIN,0); //Turn the motor off
	
	if (wiringPiSPISetup(CHANNEL, 1000000) < 0) {
		fprintf (stderr, "SPI Setup failed: %s\n", strerror (errno));
		exit(errno);
	}

	start_time=gettime();


	calibrate_sensor(1);
	


		
	
	while ( (gettime()-start_time)<=timeout) {
		move();
		delay(10);
	}

	digitalWrite(ENPIN,0); //Turn the motor off
	return 0;
}



