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

from Queue import Queue
import sys
import socket
import subprocess
import thread
import time
import os
import signal
import logging


def tcp_client(ip,tcp_port):
	#print "start tcp with" , udp_port
	stream_server = subprocess.Popen(['python', '/home/pi/petbot/tcp_client.py', ip, tcp_port], stdout=subprocess.PIPE)
	pid=stream_server.pid
	pid_q.put(pid)
	try:
		udp_port,xres,yres =  map(lambda x : int(x), stream_server.stdout.readline().strip().split())
	except:
		udp_port=0
		xres=0
		yres=0
	udp_port_q.put(udp_port)
	res_q.put((xres,yres))
	stream_server.wait()
	logging.warning("tcp client died")
	reset_q.put(pid)

def gst(xres,yres,ip,udp_port,target_bitrate):
	logging.info("Sending to UDP port %d", udp_port)
	stream_server = subprocess.Popen(['/home/pi/petbot/gst-send',str(xres), str(yres), ip, str(udp_port), str(target_bitrate)], stdout=subprocess.PIPE)
	print ['/home/pi/petbot/gst-send',str(xres), str(yres), ip, str(udp_port), str(target_bitrate)]
	pid=stream_server.pid
	pid_q.put(pid)
	stream_server.wait()
	logging.warning("gst died")
	reset_q.put(pid)


if __name__=='__main__':
	if len(sys.argv)!=3:
		print "%s ip tcp_port" % sys.argv[0]
		sys.exit(1)
	
	ip=sys.argv[1]
	tcp_port=int(sys.argv[2])
	#target_bitrate=350000
	target_bitrate=500000
	#target_bitrate=900000

	reset_q = Queue()
	pid_q = Queue()
	udp_port_q = Queue()
	res_q = Queue()

	os.system('killall -9 gst-send')

	#need to start and monitor node js on web_port with secreit
	tcp_thread=thread.start_new_thread(tcp_client,(ip,str(tcp_port)))
	udp_port=udp_port_q.get()
	xres,yres=res_q.get()
	print >> sys.stderr, "xres %d, yres %d, udp_port %d\n" % (xres,yres,udp_port)

	#need to start and monitor the gstreamer 
	if udp_port>0 and xres>0 and yres>0:
		gst_thread=thread.start_new_thread(gst,(xres,yres,ip,udp_port,target_bitrate))
		

	that_pid=reset_q.get()
	while not pid_q.empty():
		pid=pid_q.get()
		if pid!=that_pid:
			try:
				logging.warning("Killing %d" , pid)
				os.kill(pid, signal.SIGTERM)
			except:
				logging.warning("Failed to kill %d", pid)

