
import argparse
#import asyncore
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
import signal
#from gevent import monkey
#monkey.patch_all()




LOG_FILENAME="/var/log/supervisor/DEVICE_LOG"

class PetBotClient:
	def __init__(self):
		os.environ['LD_LIBRARY_PATH'] = '/usr/local/lib/'
		self.pet_selfie_filename="/home/pi/petselfie_enabled"
		self.led_auto_filename="/home/pi/led_always"
		self.t_reset=10
		self.t_count=0
		self.pet_selfie_cmd='sudo /usr/bin/nice -n 19 /home/pi/petbot-selfie/src/atos /home/pi/cnn-networks/petnet.ntwk 0 /dev/shm/out'
		self.update_lock=Lock()
		self.last_ping=-1
		self.c=0
		self.not_streaming=0
		self.selfieProcess=None
		self.state=None
		self.streamProcess=None
		self.cookieProcess=None
		self.soundProcess=None
		self.device=None
		self.enable_led_auto=None
		self.t=Timer(1.0,self.check_ping)
		self.t.start()
		self.check_wifi()
		self.state=True
		self.config={}
		self.start_time=time.time()
		self.SELFIE_TIMEIN=120
	
	def time(self):
		return time.time()-self.start_time

	def get_config(self):
		#selfie info
		_, selfie_status = self.get_selfie_status()
		self.config['selfie_status']=selfie_status
		#version info
		_, version = self.version()
		self.config['version']=version	
		#id
		self.config['id']=self.deviceID()		
		#volume
		_, volume = self.get_volume()
		self.config['volume']=volume
		return (True,self.config)

	def stop(self):
		self.state=False


	def id_generator(self,size = 6, chars = string.ascii_uppercase + string.digits):
		return "UPB_"+''.join(random.choice(chars) for _ in range(size))


	def report_dmesg(self,ip,port):
		log=subprocess.check_output(['/bin/dmesg']).split('\n')
		if len(log)>0:
			x=subprocess.Popen(['/bin/nc',str(ip),str(port)],stdin=subprocess.PIPE)
			print >> x.stdin, "\n".join(log)
			return (True,"")
		return (False,"")

	def report_log(self,ip,port):
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

	###selfie functions######
	def get_selfie_status(self):
		print >> sys.stderr,"Got request for selfie status"
		self.enable_pet_selfie=os.path.isfile(self.pet_selfie_filename)
		return (True, self.enable_pet_selfie)

	def set_selfie_status(self,enabled):
		if enabled:
			open(self.pet_selfie_filename, 'a').close()
		else:
			if os.path.isfile(self.pet_selfie_filename):
				os.remove(self.pet_selfie_filename)
		print "got request to set to ", enabled, " now is ", self.get_selfie_status()
		return (True,self.get_selfie_status()[1])

	def toggle_selfie(self):
		enabled=self.get_selfie_status()[1]
		print "toggle selfie", enabled
		self.set_selfie_status(not enabled)
		return (True,self.get_selfie_status()[1])

	####LED toggle functions########
	def get_led_status(self):
		print >> sys.stderr,"Got request for led status"
		self.enable_led_auto=os.path.isfile(self.led_auto_filename)
		return (True, self.enable_led_auto)

	def set_led_status(self,enabled):
		if enabled:
			open(self.led_auto_filename, 'a').close()
		else:
			if os.path.isfile(self.led_auto_filename):
				os.remove(self.led_auto_filename)
		print "got request to set to ", enabled, " now is ", self.get_led_status()
		return (True,self.get_led_status()[1])

	def toggle_led(self):
		enabled=self.get_led_status()[1]
		if enabled:
			subprocess.Popen(['sudo /home/pi/petbot/led/led 6 on 0'],shell=True) # turn on LED
			subprocess.Popen(['sudo /home/pi/petbot/led/led 4 on 0'],shell=True) # turn on LED
		else:
			subprocess.Popen(['sudo /home/pi/petbot/led/led 6 off 0'],shell=True) # turn off LED
			subprocess.Popen(['sudo /home/pi/petbot/led/led 4 off 0'],shell=True) # turn off LED
		print "toggle led", enabled
		self.set_led_status(not enabled)
		return (True,self.get_led_status()[1])


	def get_volume(self):
		print "GETTING SOUND"
		o=subprocess.check_output(['/usr/bin/amixer','cget','numid=1']).split('\n')
		if len(o)!=5:
			return (False,-1)
		mn=float(o[1].split(',')[3].split('=')[1])
		mx=float(o[1].split(',')[4].split('=')[1])
		c=float(o[2].split('=')[1])
		return (True,int(100*(c-mn)/(mx-mn)))

	def set_volume(self,pct):
		print "SETTING SOUND"
		o=subprocess.check_output(['/usr/bin/amixer','cset','numid=1',str(pct)+'%']).split('\n')
		if len(o)!=5:
			return (False,-1)
		mn=float(o[1].split(',')[3].split('=')[1])
		mx=float(o[1].split(',')[4].split('=')[1])
		c=float(o[2].split('=')[1])
		return (True,int(100*(c-mn)/(mx-mn)))
	
	def reboot(self):
		logging.debug('reboot')
		self.update_lock.acquire()
		try:
			reboot_info = subprocess.check_output(['/usr/bin/sudo','/sbin/reboot'])
		except Exception, err:
			print str(err)
			self.update_lock.release()
			return (False, "Failed to reboot")
		#dont release lock doing reboot!
		#update_lock.release()
		return (True, "Reboot was good")

	def update(self):
		logging.debug('startStream')
		self.update_lock.acquire()
		try:
			update_info = subprocess.check_output(['/home/pi/petbot/update.sh'],shell=True)
		except Exception, err:
			print str(err)
			self.update_lock.release()
			return (False, "failed to update")
		#update_lock.release() - if good will reboot, dont release lock!
		return (True, "update good")

	def version(self):
		print >> sys.stderr,"Got request for version"
		logging.debug('version')
		#return subprocess.check_output(['/usr/bin/git','--git-dir=/home/pi/petbot/.git/','log','-n','1'])
		try:
			h=open('/home/pi/petbot/version')
			return (True, map(lambda x : x.strip(), h.readlines()))
			h.close()
		except Exception, err:
			print str(err)
		return (False,"failed to get version")


	def deviceID(self):
		logging.debug('deviceID')
		cpu_file = open('/proc/cpuinfo','r')
		for line in cpu_file:
			info = line.split(':')
			if info and  info[0].strip() == 'Serial':
				return info[1].strip()

		raise Exception('Could not find device ID')
		return id_generator()

	def check_wifi(self):
		subprocess.Popen(['sudo /sbin/iwconfig wlan0 power off'],shell=True)
		t=Timer(20.0,self.check_wifi)
		t.start()

	def psauxf(self):
		return subprocess.check_output(['/bin/ps auxf'],shell=True)
	
	def lshome(self):
		return subprocess.check_output(['/bin/ls -l /home/pi/'],shell=True)
		

	def check_ping(self):
		if self.last_ping!=-1:
			self.t_reset=10.0 #if we already connnected check every 10 seconds
		else:
			self.t_reset=3.0 #if we are dissconnected check every 3 seconds

		if self.t_count<self.t_reset:
			self.t_count+=1 # increment clock
		else:
			self.t_count=0 # reset the clock
			ct=self.time() #current time
			if self.last_ping==-1:
				self.not_streaming=0
				logging.debug('let ping slide')
				#call blink here
				subprocess.Popen(['sudo /home/pi/petbot/led/led 6 blink 3'],shell=True)
				#timer p
			elif (ct-self.last_ping)>20:
				logging.debug('last ping was too long ago')
				if self.enable_pet_selfie and self.selfieProcess!=None:
					print "SENDING EXIT"
					print >> self.selfieProcess.stdin, "EXIT"
					print >> sys.stderr, "WAITING FOR FULL EXIT"
					self.selfieProcess.stdout.readline()
					#ping.state="STOP"
				print "last ping to long ago"
				#os._exit(1)
				#sys.exit(1)
			else:
				if self.enable_led_auto:
					subprocess.Popen(['sudo /home/pi/petbot/led/led 6 on 0'],shell=True) # turn on LED
					subprocess.Popen(['sudo /home/pi/petbot/led/led 4 on 0'],shell=True) # turn on LED
				else:
					subprocess.Popen(['sudo /home/pi/petbot/led/led 6 off 0'],shell=True) # turn off LED
					subprocess.Popen(['sudo /home/pi/petbot/led/led 4 off 0'],shell=True) # turn off LED
					
				if self.c>100:
					logging.debug('all is well ' + str(ct) + " " +str(self.last_ping))
					self.c=0
				else:
					self.c+=1
				#print "all is well",ct,last_ping[0]
				#pass
		t=Timer(1.0,self.check_ping)
		t.start()

	def ping(self):
		print "PING! %d %d" % (self.not_streaming, self.time())
		self.last_ping=self.time()
		if self.streamProcess!=None and self.streamProcess.poll()==None:
			self.not_streaming=self.time()
		else:
			#self.not_streaming+=1
			if self.enable_pet_selfie and self.time()-self.not_streaming>self.SELFIE_TIMEIN and self.state:
				if self.selfieProcess==None or self.selfieProcess.poll()!=None: #not started or dead
					print "Starting selfie process!"
					self.selfieProcess=subprocess.Popen(self.pet_selfie_cmd,stdin=subprocess.PIPE,stdout=subprocess.PIPE,shell=True)
				print >> self.selfieProcess.stdin, "GO"
				print >> self.selfieProcess.stdin, "GO"
				self.state="GO"
		return True


	def startStream(self,stream_port):
		self.not_streaming=self.time()
		print >> sys.stderr, "START STREAM!"
		logging.debug('startStream')
		if self.enable_pet_selfie and self.selfieProcess!=None:
			print "SENDING STOP"
			print >> self.selfieProcess.stdin, "STOP"
			print >> sys.stderr, "WAITING FOR FULL STOP"
			self.selfieProcess.stdout.readline()
			self.state="STOP"
			print >> sys.stderr, "STOPPPED"
		try:
			subprocess.check_output(['/usr/bin/killall','gst-manager'])
		except:
			print "DID NOT KILL OLD GST"
		try:
			self.streamProcess=subprocess.Popen(['/home/pi/petbot/gst-manager', '162.243.126.214', str(stream_port)])
			return True
		except:
			return False


	def sendCookie(self):
		logging.debug('sendCookie')
		if self.cookieProcess and self.cookieProcess.poll() == None:
			#raise Exception('Cookie drop already in progress.')
			return False

		logging.debug('sendCookie - sent')
		subprocess.Popen(['sudo /home/pi/petbot/led/led 4 blink 6'],shell=True)
		self.cookieProcess = subprocess.Popen(['/home/pi/petbot/single_cookie/single_cookie', '10'])
		return True	



	def getSounds(self):
		logging.debug('getSounds')
		ls = listdir('/home/pi/petbot/sounds')
		ls.sort()
		l = map(lambda x : x.replace('.mp3',''), ls)
		return l


	def playSound(self,url):
		logging.debug('playSound')
		if self.soundProcess and self.soundProcess.poll() == None:
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
		url=url.replace('get_sound/','get_sound_pi/'+self.deviceID()+'/')
		#playSound.process = subprocess.Popen(['-c','/usr/bin/curl ' + url + ' | /usr/bin/mpg123 -'],shell=True)
		self.soundProcess = subprocess.Popen(['/home/pi/petbot/play_sound.sh',url])
		logging.debug('playSound - playing')
		return True



	def sleepPing(self,i):
		sleep(i)
		return "response from sleepPing " + str(i)


	def connect(self,host, port):
		logging.info('Setting up command listener.')	
		self.device = DeviceListener(host, port, self.deviceID())

		logging.info('Registering device functions.')
		#petbot id
		self.device.register_function(self.deviceID)
		self.device.register_function(self.get_config)

		#wifi methods
		self.device.register_function(getWifiNetworks)
		self.device.register_function(connectWifi)

		#stream methods
		self.device.register_function(self.startStream)
		self.device.register_function(self.sendCookie)
		self.device.register_function(self.getSounds)
		self.device.register_function(self.playSound)
		self.device.register_function(self.set_volume)
		self.device.register_function(self.get_volume)

		#selfie methods
		self.device.register_function(self.set_selfie_status)
		self.device.register_function(self.get_selfie_status)
		self.device.register_function(self.toggle_selfie)
		#led methods
		self.device.register_function(self.set_led_status)
		self.device.register_function(self.get_led_status)
		self.device.register_function(self.toggle_led)
		
		self.device.register_function(self.ping)
		self.device.register_function(self.sleepPing)

		self.device.register_function(self.lshome)
		self.device.register_function(self.psauxf)
		

		self.device.register_function(self.version)
		self.device.register_function(self.update)
		self.device.register_function(self.reboot)
		self.device.register_function(self.report_log)
		self.device.register_function(self.report_dmesg)

		#connect
		logging.info("Listening for commands from server.")
		self.last_ping=self.time()

		self.device.loop(self)
		logging.warning('Disconnected from command server')

pbc=PetBotClient()

def signal_handler(signal, frame):
        print('You pressed Ctrl+C!')
	pbc.stop()
        sys.exit(0)
signal.signal(signal.SIGINT, signal_handler)


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


	pbc.get_led_status()
	if pbc.enable_led_auto: 
		subprocess.Popen(['sudo /home/pi/petbot/led/led 6 on 0'],shell=True) # turn on LED
		subprocess.Popen(['sudo /home/pi/petbot/led/led 4 on 0'],shell=True)
	else:
		subprocess.Popen(['sudo /home/pi/petbot/led/led 6 off 0'],shell=True) # turn on LED
		subprocess.Popen(['sudo /home/pi/petbot/led/led 4 off 0'],shell=True)
		

	#start timer
	#start petselfie
	pbc.get_selfie_status()
	#if ping.enable_pet_selfie:
	#	ping.selfie=subprocess.Popen(pet_selfie_cmd,stdin=subprocess.PIPE,stdout=subprocess.PIPE,shell=True)
	#	print >> ping.selfie.stdin, "GO"
	#	ping.state="GO"
	while True:
		for x in range(10):
			print "CONNECTING"
			pbc.last_ping=-1
			pbc.connect(args.host, args.port)
			logging.warning("failed to connect")
			if pbc.last_ping==-1:
				pbc.last_ping=-1
				pbc.t_count=pbc.t_reset
			pbc.not_streaming=0
			time.sleep(3+2*x)
		#try to reset the network adapter?
