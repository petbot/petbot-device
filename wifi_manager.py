#!/usr/bin/python


import sys
import tempfile
import subprocess
from wifi import Cell, Scheme
from wifi.utils import print_table, match as fuzzy_match
from wifi.exceptions import ConnectionError, InterfaceError
import logging
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
def connectWifi(network, password):
	p1=subprocess.Popen(['sudo','/sbin/ifdown', 'wlan0'],stdout=subprocess.PIPE)
	f=open('/home/pi/petbot/wifi.conf','w')
	p2=subprocess.Popen(['sudo','/usr/bin/wpa_passphrase', network, password ],stdout=f)
	p1.wait()
	p2.wait()
	p3=subprocess.Popen(['sudo','/sbin/ifup', 'wlan0'],stdout=subprocess.PIPE)
	p3.wait()
	time.sleep(3)
	p4=subprocess.Popen(['sudo','/sbin/wpa_cli', 'status'],stdout=subprocess.PIPE)
	for line in p4.stdout.readlines():
		#print line
		if line.find('wpa_state')>=0 and line.find('COMPLETED')>=0:
			return True
				
	
	'''logging.info("Connecting to wifi %s %s", network, password)
	tf=tempfile.NamedTemporaryFile()
	networks_file='/etc/network/interfaces' # tf.name
	print tf.name
	scheme_class = Scheme.for_file(networks_file)
	if scheme_class.find('wlan0', 'default'): #todo sometimes /etc/network/interfaces gets into state that cannot be read without error here
		saved_profile = Scheme.find('wlan0', 'default')
		if saved_profile:
			saved_profile.delete()
	try:
		network_cell = Cell.where('wlan0', lambda cell: cell.ssid == network)[0]
	except IndexError:
		return False
	scheme = scheme_class.for_cell('wlan0','default',network_cell,password)
	scheme.save()
	logging.info("Network saved. Attempting to connect.")
	try:
		scheme.activate()
	except:
		return False
	return True'''
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
