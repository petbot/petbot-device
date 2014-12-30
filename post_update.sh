#this gets run after the update
sudo ln -s /home/pi/petbot/timeout_watch.sh /etc/init.d/timeout_watch.sh
sudo update-rc.d timeout_watch.sh defaults

sudo update-rc.d sendmail disable



#make sure we have the right v4l-utils installed 
dpkg -s v4l-utils | grep "1.7.0-3"
if [ $? -ne 0 ] ; then
	sudo apt-get install --force-yes -y libjpeg62 libjpeg62-dev
	sudo dpkg -i /home/pi/petbot/v4l-utils_1.7.0-3_armhf.deb
fi


sudo rpi-update `cat /home/pi/petbot/kernel`
