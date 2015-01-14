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
#include <signal.h>
#include <unistd.h>
#include <semaphore.h>
#include <errno.h>
#include <math.h>

#include <time.h>
#include <wiringPi.h>
#include <wiringPiSPI.h>

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

#define CHANNEL 0

#define M1PIN	27
#define M2PIN	7
#define ENPIN	22

#define TURN_BACK 50

long samples=0;
uint8_t buf[2];
char spos[] = "7=0.063\n";
char epos[] = "7=0.21\n";
char stop[] = "7=0\n";

long start_time=0;

int dropped=0;
int to_drop=0;
int timeout=0;
int exit_now=0;

float stddev;
float mean; 
#define SMALL_AVERAGE_SIZE 20
#define AVERAGE_SIZE (9*SMALL_AVERAGE_SIZE)
float sensor_readings[AVERAGE_SIZE];
float sensor_small_averages[AVERAGE_SIZE];
float running_average=0.0;
float running_small_average=0.0;
int sensor_index=0;

#define RES	50


sem_t s_spi;
sem_t s_calibrated;
sem_t s_drop;
sem_t s_exit;

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
    //std_deviation = MAX(sqrt(variance),1);
    //fprintf(stderr,"%f, %f, %f var, %f\n",sum, average,variance,std_deviation);
    //std_deviation = MIN(std_deviation,30);
    std_deviation = 2.5;
    *out_mean=average;
    *out_stddev=std_deviation;
 
    return std_deviation;
}


unsigned int sample(int channel) {
	samples++;
	uint8_t spiData [2] ;
	uint8_t chanBits ;

	if (channel == 0)
		chanBits = 0b11010000 ;
	else
		chanBits = 0b11110000 ;

	spiData [0] = chanBits ;
	spiData [1] = 0 ;

	//sem_wait(&s_spi);//grab lock
	wiringPiSPIDataRW (0, spiData, 2) ;
	//sem_post(&s_spi);//release lock
	
	return ((spiData [0] << 7) | (spiData [1] >> 1)) & 0x3FF ;
}

void calibrate_sensor(int aq) {
	//calibrate the sensor
	if (aq==1) {
		int i=0;
		for (i=0; i<AVERAGE_SIZE; i++) {
			sensor_readings[i] = sample(0);
			delay(1);
		}
		for (i=0; i<AVERAGE_SIZE; i++) {
			sensor_readings[i] = sample(0);
			sensor_small_averages[i]=0.0;		
			delay(1);
		}
	}
	update_stats(sensor_readings,AVERAGE_SIZE,&mean,&stddev);
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
		if (exit_now==1) {
			return;
		}
		unsigned int current = sample(1);
		int adj=5*jam2;
		if ( (i>15 && current<(560-adj)) || (i>50 && current <(620-adj)) || (i>80 && current <(700-adj)) || (i>100 && current <(805-adj)) ||  (i>150 && current<(840-adj)) || (i>200 && current<(850-adj))) {
			digitalWrite(ENPIN,0);	
			turn_back();
			//printf("sample=%d %d\n", current, i); //get the read from the motor
			//printf("jam2\n");
			jam2+=1;
			int j;
			/*for (j=0; j<20*RES; j++) {
				unsigned int sensor2 = sample(0);
				if (abs(sensor2-mean)>6*stddev) {
					printf("COOKIE DROP!\n");
					digitalWrite(ENPIN,0);	
					exit(1);
				}
			}*/
			state=1-state;
			return;
		}
		delay(1);
			/*unsigned int sensor2 = sample(0);
			if (abs(sensor2-mean)>6*stddev) {
				digitalWrite(ENPIN,0);	
				turn_back();
				printf("COOKIE DROP!\n");
				exit(1);
			}*/
	}
	printf("spun up\n");

	//unsigned int prev=0x4FF;
	//int init_back_down=3;
	int init_back_down=10;
	int back_down=init_back_down;
	for (i=0; i<1000; i++) {
		if (exit_now==1) {
			return;
		}
		delay(1);
		if (exit_now==1) {
			return;
		}
		unsigned int current = sample(1);
		/*unsigned int sensor = sample(0);
		printf("sample=%d %d\n", current, sensor); //get the read from the motor
		if (abs(sensor-mean)>6*stddev) {
			unsigned int sensor2 = sample(0);
			if (abs(sensor2-mean)>6*stddev) {
				digitalWrite(ENPIN,0);	
				turn_back();
				printf("sample=%d vs sensor=%d\n", current, sensor); //get the read from the motor
				printf("COOKIE DROP!\n");
				exit(1);
			}
			
		}*/	
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
				//printf("sample=%d vs sensor=%d\n", current, sensor); //get the read from the motor
			//if (current<545) { //1.8+1.5ohm
			//if (current<755) { //1.8ohm
			//if (current<755) { //1.5ohm
			//if (current<600) { //10ohm
				back_down-=10;
			}
			if (back_down<=0) {
				digitalWrite(ENPIN,0);	
				turn_back();
				/*printf("sample=%d vs sensor=%d\n", current, sensor); //get the read from the motor
				printf("jam\n");
				int j;
				for (j=0; j<8*RES; j++) {
					unsigned int sensor2 = sample(0);
					if (abs(sensor2-mean)>6*stddev) {
						digitalWrite(ENPIN,0);	
						printf("COOKIE DROP!\n");
						exit(1);
					}
				}*/
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


void signal_callback_handler(int signum) {
   printf("Caught signal %d\n",signum);
   digitalWrite(ENPIN,0); //Turn the motor off
   exit(signum);
}


void * motor_control(void * x) {
	sem_wait(&s_calibrated);

	while (exit_now==0) {
		move();
		delay(1);
	}

	if (dropped==to_drop) {
		digitalWrite(ENPIN,0); //Turn the motor off
		turn_back();
	}
	digitalWrite(ENPIN,0); //Turn the motor off
	fprintf(stderr,"MOTOR OUT\n");
	sem_post(&s_exit);
}


float sensor_avg(float new_reading) {
	if (running_average<0.000001) {
		sensor_readings[sensor_index]=new_reading;
		int i;
		running_average=0;
		for (i=0; i<AVERAGE_SIZE; i++ ) {
			running_average+=sensor_readings[i];
		}
		running_average/=AVERAGE_SIZE;

		running_small_average=0;
		for (i=0; i<SMALL_AVERAGE_SIZE; i++) {
			running_small_average+=sensor_readings[(sensor_index+AVERAGE_SIZE-i)%AVERAGE_SIZE];
		}
		running_small_average/=SMALL_AVERAGE_SIZE;
	} else {
		running_average-=sensor_readings[sensor_index]/AVERAGE_SIZE;
		running_average+=new_reading/AVERAGE_SIZE;

		running_small_average-=sensor_readings[(sensor_index+AVERAGE_SIZE-SMALL_AVERAGE_SIZE)%AVERAGE_SIZE]/SMALL_AVERAGE_SIZE;
		running_small_average+=new_reading/SMALL_AVERAGE_SIZE;

		sensor_readings[sensor_index]=new_reading;
	}
	sensor_small_averages[sensor_index]=running_small_average;
	sensor_index=(sensor_index+1)%AVERAGE_SIZE;


	if (running_average>800) {
		stddev=1.5;
	} else if (running_average>755) {
		stddev=0.8;
	} else if (running_average>735) {
		stddev=0.5;
	} else if (running_average>725) {
		stddev=0.3;
	} else if (running_average>705) {
		stddev=0.2;
	} else {
		stddev=0.15;
	}
	return running_average;
}

void * sensor_control(void * x )  {
	calibrate_sensor(1);
	sem_post(&s_calibrated);


	int j=0;
	while (exit_now==0) {
		unsigned int sensor = sample(0);
		float v = sensor_avg(sensor);
		float mid = sensor_small_averages[(sensor_index+AVERAGE_SIZE/2+SMALL_AVERAGE_SIZE/2)%AVERAGE_SIZE];
		float start = sensor_small_averages[(sensor_index+1)%AVERAGE_SIZE];
		float now = running_small_average;
		float d1 = mid- start;
		float d2 = mid- now;
		if (j%20==0) {
			//fprintf(stderr,"RAVG3 %f | %f\n",now, stddev);
		}
		//fprintf(stderr,"RAVG3 %f %f %f %f | %f\n",running_average,start, mid, now, stddev);
		//fprintf(stderr,"RAVG3 %f | %f\n",now, stddev);
		//if (j++%5==0) {
		//	delay(1);
		//}
		if (start>0 && abs(d1)>5*stddev  && abs(d2)>5*stddev && d1*d2>0) {
			fprintf(stderr,"RAVG3 %f %f %f %f |%f\n",running_average,start, mid, now,stddev);
			dropped++;
			exit_now=1;
			if (dropped==to_drop) {
				sem_post(&s_drop);
			}

			//fprintf(stderr,"DROP! %f %f %f\n",running_small_average,running_average, stddev);
		}
		delay(1);	
	}	
	fprintf(stderr,"SENSOR OUT\n");
	sem_post(&s_exit);
}


int main (int argc, char ** argv) {

	long start = time(NULL);	
	signal(SIGINT, signal_callback_handler);
	signal(SIGTERM, signal_callback_handler);

	int r =sem_init(&s_spi, 0, 1);
	if (r<0) {
		fprintf(stderr,"Failed to init s_spi\n");
		exit(1);
	}
	r =sem_init(&s_calibrated, 0, 0);
	if (r<0) {
		fprintf(stderr,"Failed to init s_spi\n");
		exit(1);
	}
	r =sem_init(&s_drop, 0, 0);
	if (r<0) {
		fprintf(stderr,"Failed to init s_spi\n");
		exit(1);
	}
	r =sem_init(&s_exit, 0, 0);
	if (r<0) {
		fprintf(stderr,"Failed to init s_spi\n");
		exit(1);
	}

	if (argc!=2) {
		printf("%s timeout\n",argv[0]);
		exit(1);
	}

	to_drop=1;
	timeout = atoi(argv[1]);

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


        pthread_t motor_thread, sensor_thread;
        int  iret1, iret2;
        //start the motor_thread
        iret1 = pthread_create( &motor_thread, NULL, motor_control, NULL);
        if(iret1) {
                fprintf(stderr,"Error - pthread_create() return code: %d\n",iret1);
                exit(EXIT_FAILURE);
        }
        //start the sensor
        iret1 = pthread_create( &sensor_thread, NULL, sensor_control, NULL);
        if(iret1) {
                fprintf(stderr,"Error - pthread_create() return code: %d\n",iret1);
                exit(EXIT_FAILURE);
        }

	fprintf(stderr,"TIMEOUT IS %d\n",timeout);
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	ts.tv_sec+=timeout;
	int s = sem_timedwait(&s_drop,&ts);
	if (s==-1) {
		perror("what");
		fprintf(stderr,"TIMEOUT! %s\n",errno==ETIMEDOUT ? "YES" : "NO");
	} else {
		fprintf(stderr,"DROPPED A TREAT!\n");
	}

	exit_now=1;

	sem_wait(&s_exit);
	sem_wait(&s_exit);

	long end=time(NULL);
	
	float sps=((double)samples)/(end-start);
	fprintf(stderr,"SPS %f\n",sps);
	return 0;
}



