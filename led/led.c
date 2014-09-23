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
		v++;
		if (v==0) {
			times--;
		}
		if (v>=110) {
			v=-80;
		}
		if (abs(v)>50) {
			v+=4;
		}
		softPwmWrite(pin, abs(v) + 10);
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

	if (argc!=4) {
		fprintf(stderr,"%s pin [on,off,blink,glow,fixed] time\n",argv[0]);
		exit(1);
	}
	pin=atoi(argv[1]);
	char * setting=argv[2];
	times=atoi(argv[3]);



	if (strcasecmp(setting,"glow")==0) {
		softPwmCreate (pin, 0, 100);
		glow();
	} else if (strcasecmp(setting,"blink")==0) {
		softPwmCreate (pin, 0, 100);
		blink();
	} else if (strcasecmp(setting,"on")==0) {
		pinMode(pin,OUTPUT);
		digitalWrite(pin,1);	
	} else if (strcasecmp(setting,"off")==0){ 
		pinMode(pin,OUTPUT);
		digitalWrite(pin,0);	
	} else {
		softPwmCreate (pin, 0, 100);
		int v=atoi(setting);
		fixed(v);
	}
	return 0;
}


