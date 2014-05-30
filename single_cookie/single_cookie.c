#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/time.h>

#include <wiringPi.h>
#include <wiringPiSPI.h>

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

#define CHANNEL 0

uint8_t buf[2];
char spos[] = "7=0.063\n";
char epos[] = "7=0.21\n";
char stop[] = "7=0\n";

long start_time=0;

long gettime() {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	//return (tv.tv_sec) * 1000 + (tv.tv_usec) / 1000 ;
	return tv.tv_sec;
}

void setpos(char * s) {
	FILE * fptr = fopen("/dev/pi-blaster","a");
	fprintf(stderr,"ABOUT TO WRITE\n");	
	if (fptr==NULL) {
		fprintf(stderr,"failed to open file\n");
		exit(1);
	}
	fwrite(s,strlen(s),1,fptr);
	fclose(fptr);
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
	setpos(stop);
	exit(1);
}



int state=0;

void move() {
	if (state==0) {
		setpos(epos);
		state=1;
	} else {
		setpos(spos);
		state=0;
	}
	int res=50;
	int i;
	unsigned int prev=0x4FF;
	int back_down=3;
	for (i=0; i*res<1000; i++) {
		delay(res);
		unsigned int current = sample(1);
		printf("sample=%d\n", current); //get the read from the motor
		unsigned int mx=MAX(current,prev);
		unsigned int mn=MIN(current,prev);
		if (mx-mn<10) {
			back_down--;
			if (back_down==0) {
				printf("jam\n");
				return;
			}
		} else {
			back_down=3;
		}
		prev=current;
	}
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

	wiringPiSetup ();
	wiringPiISR (3, INT_EDGE_FALLING, &cookieInterrupt);

	if (wiringPiSPISetup(CHANNEL, 1000000) < 0) {
		fprintf (stderr, "SPI Setup failed: %s\n", strerror (errno));
		exit(errno);
	}

	start_time=gettime();

	while ( (gettime()-start_time)<=timeout) {
		move();
	}

	return 0;
}



