#!/bin/bash

ssid=${1:?"Network name not provided"}
password=${2:?"Password not provided"}

ifconfig wlan0 up
wpa_passphrase $ssid $password > /home/petbot/home_wifi.conf
killall wpa_supplicant
wpa_supplicant -B -i wlan0 -c /home/petbot/home_wifi.conf
dhcpcd wlan0

