#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

#include <wiringPi.h>
#include <softPwm.h>


int main (int argc, char ** argv) {

	if (wiringPiSetup () < 0) {
		fprintf (stderr, "Unable to setup GPIO: %s\n", strerror (errno)) ;
		return 1 ;
	}
	int pin=atoi(argv[1]);
	int setting=atoi(argv[2]);
	softPwmCreate (pin, 0, 100);
	softPwmWrite(pin,setting);
	for (;;) delay (1000) ;
}


