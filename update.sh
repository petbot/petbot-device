#!/bin/bash


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


wget -N https://github.com/petbot/petbot-device/archive/master.zip -P /tmp

unzip -o /tmp/master.zip -d /home/pi
if (( $? )); then
	echo "Unzipping of repository failed. Exiting."
	exit 1
fi

sudo rm -rf /home/pi/petbot
mv -T /home/pi/petbot-device-master /home/pi/petbot

