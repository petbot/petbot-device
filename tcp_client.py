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

import socket
import subprocess
import thread
import time
import logging
import sys

if len(sys.argv)!=3:
	print "%s ip tcp_port" % sys.argv[0]
	sys.exit(1)

ip=sys.argv[1]
tcp_port=int(sys.argv[2])


s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.setsockopt(socket.SOL_SOCKET, \
                 socket.SO_KEEPALIVE, 1)
s.settimeout(5.0)
s.connect((ip, tcp_port))
udp_port = int(s.recv(9))
xres = int(s.recv(9))
yres = int(s.recv(9))
print udp_port, xres,yres
print >> sys.stderr, udp_port, xres,yres
sys.stdout.flush()

while True:
	try:
		time.sleep(1.2)
		if (s.recv(4)!="ping"):
			logging.error("Did not get ping, exiting")
			break
		#print "ping"
		if (s.send("pong")!=4):
			logging.error("Did not send pong, exiting")
			break
	except Exception, err:
		logging.error("got exception")
		logging.error(str(err))
		break
	#try:
	#	r=s.recv(1024)
	#	if len(r)==0:
	#		logging.error("tcp-client -> broken pipe")
	#		break
	#except:
	#	logging.error("tcp-client -> timeout")
	#	break

s.close()


