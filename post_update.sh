#this gets run after the update
sudo ln -s /home/pi/petbot/timeout_watch.sh /etc/init.d/timeout_watch.sh
sudo update-rc.d timeout_watch.sh defaults
