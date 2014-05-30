<?php
	$networks = explode("\n", `sudo iwlist wlan0 scan | sed -n 's/\s*ESSID:"\([^"]\+\)"/\\1/p'`, -1);
	echo json_encode($networks);
?>
