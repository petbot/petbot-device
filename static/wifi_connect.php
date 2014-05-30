<?php

exec("sudo /srv/cgi-bin/wifi_connect.sh ${_POST['ssid']} ${_POST['password']}");

?>
