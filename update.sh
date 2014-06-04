#!/bin/bash

wget -N https://github.com/petbot/petbot-device/archive/master.zip -P /tmp

unzip -o /tmp/master.zip -d /home/pi
if (( $? )); then
	echo "Unzipping of repository failed. Exiting."
	exit 1
fi

sudo rm -rf /home/pi/petbot
mv -T /home/pi/petbot-device-master /home/pi/petbot

