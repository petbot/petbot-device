#!/bin/bash

if [ "$1" = "stop" ]; then
	exit
fi


sudo /home/pi/petbot/timeout_watch_helper.sh  &

