#!/usr/bin/python

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
	i=0
	scanned=False
	while not scanned and i<3:
		l=[]
		try:
			for cell in Cell.all('wlan0'):
				try:
					name=str(cell.ssid)
					if len(name.strip())>1:
						enc="None"
						if cell.encrypted:
							enc=cell.encryption_type
						l.append((cell.signal,cell.encrypted,cell.ssid,enc))
						
				except:
					logging.warning("Failed to use cell, %s" , cell.ssid)
			scanned=True
		except: # please lets move this (wpa_cli scan / wpa_scan_results ? )
			i+=1
			time.sleep(1)
	l.sort(reverse=True)
	print l
	return l


#connect to wifi network with password
ssid_exp = re.compile('^\s*ssid=.*', re.MULTILINE)
psk_exp = re.compile('^\s*psk=.*', re.MULTILINE)
wepkey_exp = re.compile('^\s*wep_key0=.*', re.MULTILINE)
def connectWifi(network, password):
	networks=getWifiNetworks()
	network_encryption_type="None"
	for signal,encrypted,ssid,encryption_type in networks:
		if ssid==network:
			print signal,encrypted,ssid,encryption_type
			network_encryption_type=encryption_type	
	logging.info("Connecting to wifi %s %s", network, password)

	subprocess.check_call(['ifdown', 'wlan0'])
	print network_encryption_type	
	if network_encryption_type=='wep':
		# get wifi configuration template	
		template_file = open('/home/pi/petbot/configs/wifi_wep.conf_template', 'r')
		wifi_conf = template_file.read()
		template_file.close()
		
		# replace network and password values
		wifi_conf=re.sub(ssid_exp, "	ssid=\""+network+"\"", wifi_conf)
		wifi_conf=re.sub(wepkey_exp, "	wep_key0=\""+password+"\"", wifi_conf)

		# write configuration
		conf_file = open('/home/pi/wifi.conf', 'w')
		conf_file.write(wifi_conf)
		conf_file.close()

	elif network_encryption_type=="None":
		# get wifi configuration template	
		template_file = open('/home/pi/petbot/configs/wifi_unprotected.conf_template', 'r')
		wifi_conf = template_file.read()
		template_file.close()
		
		# replace network and password values
		wifi_conf=re.sub(ssid_exp, "	ssid=\""+network+"\"", wifi_conf)

		# write configuration
		conf_file = open('/home/pi/wifi.conf', 'w')
		conf_file.write(wifi_conf)
		conf_file.close()
	else:
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
