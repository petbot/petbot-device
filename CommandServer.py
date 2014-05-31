
import asynchat
import socket
import xmlrpclib
from SimpleXMLRPCServer import SimpleXMLRPCDispatcher
import sys
#from gevent import monkey
#monkey.patch_all()

import time


NAME_FIELD_LENGTH = 4

class CommandListener(asynchat.async_chat, SimpleXMLRPCDispatcher):

	def __init__(self, host, port, name):
		
		SimpleXMLRPCDispatcher.__init__(self,allow_none=True)

		# connect to server to listen for commands
		asynchat.async_chat.__init__(self)
                #http://asyncspread.googlecode.com/svn-history/r107/trunk/asyncspread/connection.py
                self.keepalive = True
                self.keepalive_idle = 3 # every 10 seconds of idleness, send a keepalive (empty PSH/ACK)
                self.keepalive_maxdrop = 5 # one minute
                self.create_socket(socket.AF_INET, socket.SOCK_STREAM)
                self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1) # enable
                self.socket.setsockopt(socket.SOL_TCP, socket.TCP_KEEPIDLE, self.keepalive_idle)
                self.socket.setsockopt(socket.SOL_TCP, socket.TCP_KEEPINTVL, self.keepalive_idle)
              	self.socket.setsockopt(socket.SOL_TCP, socket.TCP_KEEPCNT, self.keepalive_maxdrop)
		self.settimeout(3)		
		self.connect((host,port))
		# device identification
		self.name = name

		# setup chat for collection of header data
		self.input_buffer = ''
		self.header = True
		self.set_terminator(NAME_FIELD_LENGTH)


	def collect_incoming_data(self, data):
		self.input_buffer += data

	def found_terminator(self):
		# get header data or command
		if self.header:
			message_length = int(self.input_buffer)
			self.set_terminator(message_length)
		else:
			# send command to supervisors xmlrpc port
			command_response = self._marshaled_dispatch(self.input_buffer)
			message =''.join(['%0.4d' % len(command_response), command_response])
			self.send(message)
			self.set_terminator(NAME_FIELD_LENGTH)

		# reset buffer and switch from collecting header / data
		self.input_buffer = ''
		self.header = not self.header

	def handle_close(self):
		print >> sys.stderr, "CLosing"
		self.close()

	def handle_error(self):
		print >> sys.stderr, "ERRROR"
		print time.strftime("%c"),"An error has occured" 
		asynchat.async_chat.handle_error(self)

	def handle_close(self):
		print >> sys.stderr, "ERRROR"
		print time.strftime("%c"),"A close has occured" 
		asynchat.async_chat.handle_close(self)
