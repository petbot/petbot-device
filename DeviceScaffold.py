
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
from threading import Timer, Lock
import sys
import os
import RPi.GPIO as GPIO
#from gevent import monkey
#monkey.patch_all()

t_reset=[0]
t_count=[0]


LOG_FILENAME="/var/log/supervisor/DEVICE_LOG"

def id_generator(size = 6, chars = string.ascii_uppercase + string.digits):
	return "UPB_"+''.join(random.choice(chars) for _ in range(size))

update_lock=Lock()

def report_dmesg(ip,port):
	log=subprocess.check_output(['/bin/dmesg']).split('\n')
	if len(log)>0:
		x=subprocess.Popen(['/bin/nc',str(ip),str(port)],stdin=subprocess.PIPE)
		print >> x.stdin, "\n".join(log)
		return (True,"")
	return (False,"")

def report_log(ip,port):
	log=[]
	for file in os.listdir('/var/log/supervisor/'):
		try:
			log+=["FILE\t"+file+'\n']
			h=open('/var/log/supervisor/'+file,'r')
			lines=h.readlines()
			log+=lines
			h.close()
			print >> sys.stderr, "IN REPORT LOG",file, len(lines)
		except:
			print >> sys.stderr, "FAILED TO OPEN",file
	if len(log)>0:
		x=subprocess.Popen(['/bin/nc',str(ip),str(port)],stdin=subprocess.PIPE)
		print >> x.stdin, "".join(log)
		return (True,"")
	return (False,"")

def get_volume():
	print "GETTING SOUND"
	o=subprocess.check_output(['/usr/bin/amixer','cget','numid=1']).split('\n')
	if len(o)!=5:
		return (False,-1)
	mn=float(o[1].split(',')[3].split('=')[1])
	mx=float(o[1].split(',')[4].split('=')[1])
	c=float(o[2].split('=')[1])
	return (True,int(100*(c-mn)/(mx-mn)))

def set_volume(pct):
	print "SETTING SOUND"
	o=subprocess.check_output(['/usr/bin/amixer','cset','numid=1',str(pct)+'%']).split('\n')
	if len(o)!=5:
		return (False,-1)
	mn=float(o[1].split(',')[3].split('=')[1])
	mx=float(o[1].split(',')[4].split('=')[1])
	c=float(o[2].split('=')[1])
	return (True,int(100*(c-mn)/(mx-mn)))
	
def reboot():
	logging.debug('reboot')
	update_lock.acquire()
	try:
		reboot_info = subprocess.check_output(['/usr/bin/sudo','/sbin/reboot'])
	except Exception, err:
		print str(err)
		update_lock.release()
		return (False, "Failed to reboot")
	#dont release lock doing reboot!
	#update_lock.release()
	return (True, "Reboot was good")

def update():
	logging.debug('startStream')
	update_lock.acquire()
	try:
		update_info = subprocess.check_output(['/home/pi/petbot/update.sh'],shell=True)
	except Exception, err:
		print str(err)
		update_lock.release()
		return (False, "failed to update")
	#update_lock.release() - if good will reboot, dont release lock!
	return (True, "update good")

def version():
	logging.debug('version')
	#return subprocess.check_output(['/usr/bin/git','--git-dir=/home/pi/petbot/.git/','log','-n','1'])
	try:
		h=open('/home/pi/petbot/version')
		return (True, map(lambda x : x.strip(), h.readlines()))
		h.close()
	except Exception, err:
		print str(err)
	return (False,"failed to get version")


def deviceID():
	logging.debug('deviceID')
	cpu_file = open('/proc/cpuinfo','r')
	for line in cpu_file:
		info = line.split(':')
		if info and  info[0].strip() == 'Serial':
			return info[1].strip()

	raise Exception('Could not find device ID')
	return id_generator()

last_ping=[-1]
c=[0]
not_streaming=[0]
def check_ping():
	if last_ping[0]!=-1:
		t_reset[0]=10.0 #if we already connnected check every 10 seconds
	else:
		t_reset[0]=3.0 #if we are dissconnected check every 3 seconds

	if t_count[0]<t_reset[0]:
		t_count[0]+=1 # increment clock
	else:
		t_count[0]=0 # reset the clock
		ct=time.time() #current time
		if last_ping[0]==-1:
			not_streaming[0]=0
			logging.debug('let ping slide')
			#call blink here
			subprocess.Popen(['sudo /home/pi/petbot/led/led 6 blink 3'],shell=True)
			#timer p
		elif (ct-last_ping[0])>20:
			logging.debug('last ping was too long ago')
			if ping.selfie!=None:
				print "SENDING STOP"
				print >> ping.selfie.stdin, "STOP"
				print >> sys.stderr, "WAITING FOR FULL STOP"
				ping.selfie.stdout.readline()
				ping.state="STOP"
			os._exit(1)
			#sys.exit(1)
		else:
			subprocess.Popen(['sudo /home/pi/petbot/led/led 6 on 0'],shell=True) # turn on LED
			if c[0]>100:
				logging.debug('all is well ' + str(ct) + " " +str(last_ping[0]))
				c[0]=0
			else:
				c[0]+=1
			#print "all is well",ct,last_ping[0]
			#pass
	t=Timer(1.0,check_ping)
	t.start()

def ping():
	print "PING!",not_streaming[0]
	last_ping[0]=time.time()
	if startStream.process and startStream.process.poll()==None:
		not_streaming[0]=0
	else:
		not_streaming[0]+=1
		if not_streaming[0]>4:
			if ping.selfie==None:
				ping.selfie=subprocess.Popen('sudo /usr/bin/nice -n 19 /home/pi/petbot-selfie/src/atos /home/pi/cnn-networks/ccv2012.ntwk -2 /home/pi/cnn-networks/model_2012_l2_p5p50_n4 /dev/shm/out',stdin=subprocess.PIPE,stdout=subprocess.PIPE,shell=True)
				print >> ping.selfie.stdin, "GO"
				ping.state="GO"
			elif ping.state=="STOP":
				print >> ping.selfie.stdin, "GO"
				ping.state="GO"
			ping.state="GO"
	return True

ping.selfie=None
ping.state=None

def startStream(stream_port):
	not_streaming[0]=0
	print >> sys.stderr, "START STREAM!"
	logging.debug('startStream')
	print ping.selfie
	if ping.selfie!=None:
		print "SENDING STOP"
		print >> ping.selfie.stdin, "STOP"
		print >> sys.stderr, "WAITING FOR FULL STOP"
		ping.selfie.stdout.readline()
		ping.state="STOP"
		print >> sys.stderr, "STOPPPED"
	try:
		subprocess.check_output(['/usr/bin/killall','gst-manager'])
	except:
		print "DID NOT KILL OLD GST"
	try:
		startStream.process=subprocess.Popen(['/home/pi/petbot/gst-manager', '162.243.126.214', str(stream_port)])
		return True
	except:
		return False

startStream.process=None

def sendCookie():
	logging.debug('sendCookie')
	if sendCookie.process and sendCookie.process.poll() == None:
		#raise Exception('Cookie drop already in progress.')
		return False

	logging.debug('sendCookie - sent')
	sendCookie.process = subprocess.Popen(['/home/pi/petbot/single_cookie/single_cookie', '10'])
	return True	

sendCookie.process = None


def getSounds():
	logging.debug('getSounds')
	ls = listdir('/home/pi/petbot/sounds')
	ls.sort()
	l = map(lambda x : x.replace('.mp3',''), ls)
	return l


def playSound(url):
	logging.debug('playSound')
	if playSound.process and playSound.process.poll() == None:
		#raise Exception('Sound already playing.')
		return False
	try:
		if (int(url))==8:
			#old api for iOS
			url='https://petbot.ca/static/sounds/mpu.mp3'
	except:
		pass
	if url[:4]!='http':
		return False
	url=url.replace('get_sound/','get_sound_pi/'+deviceID()+'/')
	#playSound.process = subprocess.Popen(['-c','/usr/bin/curl ' + url + ' | /usr/bin/mpg123 -'],shell=True)
	playSound.process = subprocess.Popen(['/home/pi/petbot/play_sound.sh',url])
	logging.debug('playSound - playing')
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
	device.register_function(set_volume)
	device.register_function(get_volume)

	device.register_function(ping)
	device.register_function(sleepPing)

	device.register_function(version)
	device.register_function(update)
	device.register_function(reboot)
	device.register_function(report_log)
	device.register_function(report_dmesg)

	# register supervisord proxy to be accessible from device proxy
	logging.info('Connecting supervisord to available by device proxy.')
	supervisor_proxy = ServerProxy('http://127.0.0.1:9001')
	supervisor_proxy._dispatch = lambda method, params: getattr(supervisor_proxy, method)(*params)
	device.register_instance(supervisor_proxy)

	#connect
	logging.info("Listening for commands from server.")
	last_ping[0]=time.time()
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
	arg_parser.add_argument('-a', '--host', default = pi_server)
	arg_parser.add_argument('-p', '--port', type = int, default = pi_server_port)
	args =  arg_parser.parse_args()

	logging.basicConfig(format = '%(asctime)s\t%(levelname)s\t%(message)s',filename=LOG_FILENAME, level=logging.DEBUG)
	#logging.basicConfig(filename = "petbot.log", level = logging.DEBUG, format = '%(asctime)s\t%(levelname)s\t%(module)s\t%(funcName)\t%(message)s')

	subprocess.Popen(['sudo /home/pi/petbot/led/led 6 on 0'],shell=True) # turn on LED

	#start timer
	t_reset[0]=10
	t_count[0]=0
	t=Timer(1.0,check_ping)
	t.start()
	#start petselfie
	ping.selfie=subprocess.Popen('sudo /home/pi/petbot-selfie/src/atos /home/pi/cnn-networks/ccv2012.ntwk -2 /home/pi/cnn-networks/model_2012_l2_p5p50_n4 /dev/shm/out',stdin=subprocess.PIPE,stdout=subprocess.PIPE,shell=True)
	print >> ping.selfie.stdin, "GO"
	ping.state="GO"
	while True:
		for x in range(10):
			try:
				print "CONNECTING"
				last_ping[0]=-1
				connect(args.host, args.port)
			except:
				logging.warning("failed to connect")
			if last_ping[0]!=-1:
				last_ping[0]=-1
				t_count[0]=t_reset[0]
			not_streaming[0]=0
			time.sleep(3+2*x)
		#try to reset the network adapter?
