/*
 * button.c:
 *	Simple button test for the Quick2Wire interface board.
 *
 * Copyright (c) 2012-2013 Gordon Henderson. <projects@drogon.net>
 ***********************************************************************
 * This file is part of wiringPi:
 *	https://projects.drogon.net/raspberry-pi/wiringpi/
 *
 *    wiringPi is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU Lesser General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    wiringPi is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU Lesser General Public License for more details.
 *
 *    You should have received a copy of the GNU Lesser General Public License
 *    along with wiringPi.  If not, see <http://www.gnu.org/licenses/>.
 ***********************************************************************

#This file is part of PetBot.
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
#    along with Foobar.  If not, see <http://www.gnu.org/licenses/>.

 */

#include <unistd.h>
#include <stdio.h>
#include <wiringPi.h>

#define	BUTTON	9
#define	LED1	7

int main (void)
{

// Enable the on-goard GPIO

  wiringPiSetup () ;

  pinMode (BUTTON, INPUT) ;
  pinMode (LED1, OUTPUT) ;

  digitalWrite (LED1, 0) ;		// 2nd LED off

  for (;;)
  {
    if (digitalRead (BUTTON) == HIGH)	// Swap LED states
    {
      while (digitalRead (BUTTON) == HIGH)
	delay (5) ;
      delay (10) ;
      //fork and exec
      pid_t pid;
      pid = fork();
      if (pid==0) {
        int ret = execl("/usr/bin/mpg123","/usr/bin/mpg123","/home/pi/atosdv2/dial.mp3");
        fprintf(stdout,"playing sound\n");
      }
      delay(3000);
      pid = fork();
      if (pid==0) {
      	int ret = execl("/home/pi/atosdv2/single_cookie/single_cookie","/home/pi/atosdv2/single_cookie/single_cookie","5",(char*)0);
      	fprintf(stdout,"%d ret\n",ret);
      }
      int state=0;
      int i=0;
      for (i=0; i<10; i++) {
	      state=1-state;
	      digitalWrite(LED1,state);
	      delay(100);
      }
    }
    delay (1) ;
  }

  return 0 ;
}
