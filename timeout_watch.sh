#!/bin/bash

ping_host=petbot.ca

no_ping=0

while [ 0 -lt 1 ]; 
do
	l=`ps aux | grep DeviceScaffold.py | grep python | wc -l `
	if [ $l -lt 1 ]; then 
		sudo service supervisor start
	fi
	if [ $no_ping -gt 20 ]; then
		sudo reboot
	fi
	p=`ping -c 1 $ping_host | tail -n 2 | head -n 1 | awk '{print $4}'`
	if [ $p -ne 1 ]; 
	then
		no_ping=`expr $no_ping + 1`
	else
		no_ping=0
	fi
	sleep 5
done
