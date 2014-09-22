#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

#include <wiringPi.h>
#include <softPwm.h>

int pin=0;
int times=0;

void glow() {
	int v=0;
	int i;
	for (i=0; times>0; i++) {
		v++;
		if (v==0) {
			times--;
		}
		if (v>=110) {
			v=-80;
		}
		if (abs(v)>50) {
			v+=2;
		}
		softPwmWrite(pin, abs(v) + 20);
		delay(70);
	}

}

void blink() {
	int i;
	for (i=0; i<times*2; i++) {
		softPwmWrite(pin, i%2==0 ? 0 : 100 );
		fprintf(stderr,"Writting to pin %d, %d\n",pin,i%2==0 ? 0 : 100 );
		delay(200);
	}
}

void fixed(int v) {
	softPwmWrite(pin,v);
	fprintf(stderr,"Writting to pin %d, %d\n",pin,v);
	delay(1000);
}

int main (int argc, char ** argv) {
	if (wiringPiSetup () < 0) {
		fprintf (stderr, "Unable to setup GPIO: %s\n", strerror (errno)) ;
		return 1 ;
	}
	pin=atoi(argv[1]);
	char * setting=argv[2];

	softPwmCreate (pin, 0, 100);

	if (strcasecmp(setting,"glow")==0) {
		times=atoi(argv[3]);
		glow();
	} else if (strcasecmp(setting,"blink")==0) {
		times=atoi(argv[3]);
		blink();
	} else {
		int v=atoi(setting);
		fixed(v);
	}
	return 0;
}


