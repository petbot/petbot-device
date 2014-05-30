#!/usr/bin/python

import subprocess
import sys
import tempfile
from wifi import Cell, Scheme
from wifi.utils import print_table, match as fuzzy_match
from wifi.exceptions import ConnectionError, InterfaceError
import logging
import re
import time

#return a sorted list of networks by signal
def getWifiNetworks():
	l=[]
	for cell in Cell.all('wlan0'):
		try:
			name=str(cell.ssid)
			if len(name.strip())>1:
				l.append((cell.signal,cell.encrypted,cell.ssid))
		except:
			logging.warning("Failed to use cell, %s" , cell.ssid)
	l.sort(reverse=True)
	print l
	return l


#connect to wifi network with password
ssid_exp = re.compile('^\s*ssid=.*', re.MULTILINE)
psk_exp = re.compile('^\s*psk=.*', re.MULTILINE)
def connectWifi(network, password):

	logging.info("Connecting to wifi %s %s", network, password)

	subprocess.check_call(['ifdown', 'wlan0'])
	
	# get wifi configuration template	
	template_file = open('/home/pi/petbot/configs/wifi.conf_template', 'r')
	wifi_conf = template_file.read()
	template_file.close()
	
	# replace network and password values
	wpa_info = subprocess.check_output(['wpa_passphrase', network, password])
	wifi_conf=re.sub(ssid_exp, ssid_exp.search(wpa_info).group(0), wifi_conf)
	wifi_conf=re.sub(psk_exp, psk_exp.search(wpa_info).group(0), wifi_conf)

	# write configuration
	conf_file = open('/home/pi/wifi.conf', 'w')
	conf_file.write(wifi_conf)
	conf_file.close()

	# bring up wifi and check status
	subprocess.check_call(['ifup', 'wlan0'])
	time.sleep(3)
	wpa_status = subprocess.check_output(['wpa_cli', 'status'])
	for line in wpa_status.split('\n'):
		if line == 'wpa_state=COMPLETED':
			return True

	return False
	



if __name__=='__main__':
	if len(sys.argv)==1:
		print "%s scan | %s connect network password" % (sys.argv[0],sys.argv[0])
		sys.exit(1)
	if sys.argv[1]=='scan':
		print getWifiNetworks()
	elif sys.argv[1]=='connect':
		if len(sys.argv)!=4:
			print "%s connect network password" % sys.argv[0]
			sys.exit(1)
		network=sys.argv[2]
		password=sys.argv[3]
		connectWifi(network,password)
