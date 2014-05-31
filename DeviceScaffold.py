
import argparse
import asyncore
import logging
import string
import random
from os import listdir
from CommandServer import *
from xmlrpclib import ServerProxy
import time
from wifi_manager import *
import subprocess
from time import sleep
#from gevent import monkey
#monkey.patch_all()

def id_generator(size = 6, chars = string.ascii_uppercase + string.digits):
	return "UPB_"+''.join(random.choice(chars) for _ in range(size))


def deviceID():
	cpu_file = open('/proc/cpuinfo','r')
	for line in cpu_file:
		info = line.split(':')
		if info and  info[0].strip() == 'Serial':
			return info[1].strip()

	raise Exception('Could not find device ID')
	return id_generator()


def ping():
	return True


def startStream(stream_port):
	print "start stream"
	logging.debug('startStream')
	try: 
		subprocess.Popen(['python','/home/pi/petbot/gst-manager.py', '162.243.126.214', str(stream_port)])
		return True
	except:
		return False


def sendCookie():
	
	if sendCookie.process and sendCookie.process.poll() == None:
		#raise Exception('Cookie drop already in progress.')
		return False

	sendCookie.process = subprocess.Popen(['/home/pi/petbot/single_cookie/single_cookie', '10'])
	return True	

sendCookie.process = None


def getSounds():
	ls = listdir('/home/pi/petbot/sounds')
	ls.sort()
	l = map(lambda x : x.replace('.mp3',''), ls)
	return l


def playSound(index):

	if playSound.process and playSound.process.poll() == None:
		#raise Exception('Sound already playing.')
		return False

	ls = listdir('/home/pi/petbot/sounds')
	ls.sort()
	
	if not(0 <= index and index < len(ls)):
		return False
	
	playSound.process = subprocess.Popen(['/usr/bin/mpg123','/home/pi/petbot/sounds/' + ls[index]])
	return True

playSound.process = None


def sleepPing(i):
	sleep(i)
	return "response from sleepPing " + str(i)


def connect(host, port):

	logging.info('Setting up command listener.')	
	device = CommandListener(host, port, deviceID())

	logging.info('Registering device functions.')
	#petbot id
	device.register_function(deviceID)

	#wifi methods
	device.register_function(getWifiNetworks)
	device.register_function(connectWifi)

	#stream methods
	device.register_function(startStream)
	device.register_function(sendCookie)
	device.register_function(getSounds)
	device.register_function(playSound)

	device.register_function(ping)
	device.register_function(sleepPing)

	# register supervisord proxy to be accessible from device proxy
	logging.info('Connecting supervisord to available by device proxy.')
	supervisor_proxy = ServerProxy('http://127.0.0.1:9001')
	supervisor_proxy._dispatch = lambda method, params: getattr(supervisor_proxy, method)(*params)
	device.register_instance(supervisor_proxy)

	#connect
	logging.info("Listening for commands from server.")
	asyncore.loop(timeout=1)
	logging.warning('Disconnected from command server')

pi_server="127.0.0.1"
pi_server_port="54000"

try:
    h=open('/home/pi/petbot/petbot.conf')
    for line in h.readlines():
        line=line.split()
        if line[0]=="pi-server":
            pi_server=line[1].split(':')[0]
            pi_server_port=int(line[1].split(':')[1])
except:
    print "Could not find config file"
    sys.exit(1)


if __name__ == '__main__':

	# get command line arguments
	arg_parser = argparse.ArgumentParser(description = 'Act as device for manager on server.')
	arg_parser.add_argument('-a', '--host', default = '162.243.126.214')
	arg_parser.add_argument('-p', '--port', type = int, default = 54000)
	args =  arg_parser.parse_args()

	logging.basicConfig(format = '%(asctime)s\t%(levelname)s\t%(message)s')
	#logging.basicConfig(filename = "petbot.log", level = logging.DEBUG, format = '%(asctime)s\t%(levelname)s\t%(module)s\t%(funcName)\t%(message)s')

	while True:
		for x in range(10):
			connect(args.host, args.port)
			time.sleep(3+2*x)
		#try to reset the network adapter?
	print "CONNECTING"
