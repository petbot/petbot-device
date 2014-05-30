#!/bin/bash


###############
#INSTALL PARTS#
###############

# install pip and wifi module
wget -P /tmp https://bootstrap.pypa.io/get-pip.py
sudo python /tmp/get-pip.py
sudo pip install wifi

# supervisord
sudo apt-get -y install supervisor

# get gstreamer libraries
grep -q "deb http://vontaene.de/raspbian-updates/ . main" /etc/apt/sources.list
if [ $? -ne 0 ];
then
	echo "deb http://vontaene.de/raspbian-updates/ . main" | sudo tee -a /etc/apt/sources.list > /dev/null
fi

sudo apt-get update
sudo apt-get install --force-yes -y libgstreamer1.0-0-dbg gstreamer1.0-tools libgstreamer-plugins-base1.0-0 \
	gstreamer1.0-plugins-good gstreamer1.0-plugins-bad-dbg gstreamer1.0-omx gstreamer1.0-alsa

# wiring PI
git clone git://git.drogon.net/wiringPi ~/wiringPi
cd ~/wiringPi
sudo ./build
cd

#install mpg123
sudo apt-get install -y mpg123

#install extras
sudo apt-get install -y vim



###############
#Grab the REPO#
###############

# get petbot repo
#git clone https://github.com/misko/petbot -o github ~/petbot
if [ -e ~/petbot ]; then
	echo WARNING ABOUT TO REMOVE THE FULL REPO!
	echo press ctrl-c to cancel in next 10 seconds!
	t=10
	while [ $t -gt 0 ]; do
		echo T = $t seconds
		sleep 1
		t=`expr $t - 1`
	done
	sleep 3
	rm -rf ~/petbot
fi
git clone https://github.com/petbot/petbot-device.git -o github ~/petbot



############
#Link it up#
############

#link supervisord conf
sudo rm -f /etc/supervisor/conf.d/petbot.conf 2> /dev/null
sudo ln -s /home/petbot/supervisord.conf /etc/supervisor/conf.d/petbot.conf
sudo rm -f /etc/network/interfaces 2> /dev/null
sudo ln -s /home/petbot/configs/interfaces /etc/network/interfaces
#if not exist wifi config then make one
if [ ! -e /home/pi/petbot/wifi.conf ] ; then
	cp /home/pi/petbot/configs/wifi.conf_template /home/pi/petbot/wifi.conf
fi


##############################
#Check MD5 sums and such here#
##############################

#sudo reboot

