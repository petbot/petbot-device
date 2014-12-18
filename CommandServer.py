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

import time
import sys
#import asynchat
#import socket
import xmlrpclib
#from SimpleXMLRPCServer import SimpleXMLRPCDispatcher

#from gevent.coros import BoundedSemaphore
#import gevent 
from gevent import socket
import socket
import struct
NAME_FIELD_LENGTH = 4

#l=BoundedSemaphore(1)
import threading
#from gevent.queue import Queue

#from gevent import monkey
#monkey.patch_all()






class Worker():
	def __init__(self, host, port, name):
		#SimpleXMLRPCDispatcher.__init__(self,allow_none=True)
		self.host=host
		self.port=port
		self.name=name
		self.functions={}
		self.functions_raw=[]
		#print "worker",host, port
		#self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
		#self.sock.connect((host,port))
		self.sock=socket.create_connection((host,port))
		#self.sock=gevent.socket.create_connection((host,port))
		#print "worker",host, port,self.sock
		self.sock.settimeout(60)

	def register_function(self,f):
		self.functions[f.__name__]=f
		self.functions_raw.append(f)

	def get_len(self):
		self.l=0
		try:
			r=self.sock.recv(4)
			if r==None or len(r)==0:
				self.l=0
			self.l=int(r)
		except:
			pass
		return self.l
		#self.l=struct.unpack("I",self.sock.recv(4))[0]

	def get_req(self):
		self.r=""
		while len(self.r)<self.l:
			try:
				self.r+=self.sock.recv(self.l-len(self.r))
			except:
				return None
		return self.r

	def do_work(self):
		print "doing work", threading.current_thread()
		if self.get_req()==None:	
			print "Failed in mid request"
			return
		params,f=xmlrpclib.loads(self.r)
		ret=self.functions[f](*params)
		command_response=xmlrpclib.dumps((ret,),methodresponse=True)
		#command_response = self._marshaled_dispatch(self.r)
		message =''.join(['%0.4d' % len(command_response), command_response])
		self.sock.send(message)
		#self.sock.send(len(command_response))
		#self.sock.send(command_response)
		print "done work", threading.current_thread(),f
	
	def do_work_loop(self):
		self.do_work()
		while True:
			if self.get_len()==0:
				break
			self.do_work()
		print "thread dying", threading.current_thread()
	
	def block_for_work(self):
		if self.get_len()==0:
			print "thread dying", threading.current_thread()
			return
		self.do_work() #shoudl be call for deviceID
		if self.get_len()==0:
			print "thread dying", threading.current_thread()
			return
		#self.do_work_loop()
		#t=threading.Thread(target=connect, args=(self.host, self.port, self.name, self.functions_raw,))
		#add_thread(t)
		#t.start()
		self.spawn()
		self.do_work_loop()
	
	def spawn(self):
		t=threading.Thread(target=connect, args=(self.host, self.port, self.name, self.functions_raw,))
		#add_thread(t) #check for timeout and return?
		t.daemon=True
		t.start()
	

def connect(host,port,name,fs):
	w=Worker(host,port,name)
	for f in fs:
		w.register_function(f)
	w.block_for_work()

class DeviceListener:
	def __init__(self, host, port, name):
		self.host=host
		self.port=port
		self.name=name
		self.functions=[]
	
	def register_function(self,f):
		self.functions.append(f)

	def loop(self,pbc):
		#ws=[]
		errors=0
		while True:
			print "DOING STUFF"
			if not pbc.state:
				print "SHUTDOWN"
				return
			try:
				connect(self.host,self.port,self.name,self.functions)
				pbc.ping()
			except socket.error, s:
				print "socket error", s
				time.sleep(1)
			except:
				print "other error", sys.exc_info()[0]
				time.sleep(1)
			#ws.append(gevent.spawn(connect,self.host,self.port,self.name,self.functions))
			#t=threading.Thread(target=connect, args=(self.host, self.port, self.name, self.functions,))
			#t.start()
		print "DONE LOOP"
			

			
		
		

#class CommandListener(asynchat.async_chat, SimpleXMLRPCDispatcher):
#
#	def __init__(self, host, port, name):
#		
#		SimpleXMLRPCDispatcher.__init__(self,allow_none=True)
#
#		# connect to server to listen for commands
#		asynchat.async_chat.__init__(self)
#                #http://asyncspread.googlecode.com/svn-history/r107/trunk/asyncspread/connection.py
#                self.keepalive = True
#                self.keepalive_idle = 3 # every 10 seconds of idleness, send a keepalive (empty PSH/ACK)
#                self.keepalive_maxdrop = 5 # one minute
#                self.create_socket(socket.AF_INET, socket.SOCK_STREAM)
#                self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1) # enable
#                self.socket.setsockopt(socket.SOL_TCP, socket.TCP_KEEPIDLE, self.keepalive_idle)
#                self.socket.setsockopt(socket.SOL_TCP, socket.TCP_KEEPINTVL, self.keepalive_idle)
#              	self.socket.setsockopt(socket.SOL_TCP, socket.TCP_KEEPCNT, self.keepalive_maxdrop)
#		self.settimeout(3)		
#		self.connect((host,port))
#		# device identification
#		self.name = name
#
#		# setup chat for collection of header data
#		self.input_buffer = ''
#		self.header = True
#		self.set_terminator(NAME_FIELD_LENGTH)
#
#
#	def collect_incoming_data(self, data):
#		self.input_buffer += data
#
#	def found_terminator(self):
#		# get header data or command
#		if self.header:
#			message_length = int(self.input_buffer)
#			self.set_terminator(message_length)
#		else:
#			# send command to supervisors xmlrpc port
#			command_response = self._marshaled_dispatch(self.input_buffer)
#			message =''.join(['%0.4d' % len(command_response), command_response])
#			self.send(message)
#			self.set_terminator(NAME_FIELD_LENGTH)
#
#		# reset buffer and switch from collecting header / data
#		self.input_buffer = ''
#		self.header = not self.header
#
#	def handle_close(self):
#		print >> sys.stderr, "CLosing"
#		self.close()
#
#	def handle_error(self):
#		print >> sys.stderr, "ERRROR"
#		print time.strftime("%c"),"An error has occured" 
#		asynchat.async_chat.handle_error(self)
#
#	def handle_close(self):
#		print >> sys.stderr, "ERRROR"
#		print time.strftime("%c"),"A close has occured" 
#		asynchat.async_chat.handle_close(self)
