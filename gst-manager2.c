/*#This file is part of PetBot.
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
#    along with Foobar.  If not, see <http://www.gnu.org/licenses/>. */

#include <gst/gst.h>
#include <stdio.h>  
#include <stdlib.h> 
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <assert.h>
#include <OMX_Video.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/wait.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <arpa/inet.h>

#include <pthread.h>
#include <semaphore.h>

#include "codes.h"


guint64 bytes_sent;

sem_t tcp_client_mutex;
char * tcp_server_ip;
int tcp_server_port;
int sockfd;

char * gst_server_ip;
int gst_udp_port,gst_xres,gst_yres;
int target_bitrate=500000;
GstElement *pipeline;

sem_t restart_mutex;
int shutdown_now;

int pipe_to_gst[2];
int pipe_to_tcp[2];
int pipe_from_gst[2];
int pipe_from_tcp[2];

int tcp_status=1;
int gst_status=1;

int retries=10;

void unload_module(char * s) {
	int pid=fork();
	if (pid==0) {
		//child
		char * args[] = { "/usr/bin/sudo","/sbin/rmmod", "-f", s, NULL };
		int r = execv(args[0],args);
		fprintf(stderr,"SHOULD NEVER REACH HERE %d\n",r);
	}
	//master
	wait(NULL);
}

void load_module(char *s ) {
	int pid=fork();
	if (pid==0) {
		//child
		char * args[] = { "/usr/bin/sudo", "/sbin/modprobe", s, NULL };
		int r = execv(args[0],args);
		fprintf(stderr,"SHOULD NEVER REACH HERE %d\n",r);
	}
	//master
	wait(NULL);
}

void reload_uvc() {
	fprintf(stderr,"remogin\n");
	unload_module("uvcvideo");
	unload_module("videobuf2_core");
	unload_module("videobuf2_vmalloc");
	unload_module("videodev");
	load_module("videodev");
	load_module("videobuf2_core");
	load_module("videobuf2_vmalloc");
	load_module("uvcvideo");
	fprintf(stderr,"Loaded UVC\n");
	return;
}

void * gst_client(void * not_used );

void * retry() {
  if (retries==0 || shutdown_now==1) {
	fprintf(stderr,"REALLY GIVING UP\n");
  	sem_post(&restart_mutex);
  	return NULL;
  }
  retries-=1;
  fprintf(stderr,"Retrying internally %d\n",retries);
  return gst_client(NULL); //try again
}

void * gst_client(void * not_used ) { //(char * ip, int udp_port, int target_bitrate, int height, int width) {
  	sem_wait(&tcp_client_mutex);
	char s_xres[128],s_yres[128],s_udp_port[128],s_bitrate[128];
	snprintf(s_xres,128,"%d",gst_xres);
	snprintf(s_yres,128,"%d",gst_yres);
	snprintf(s_udp_port,128,"%d",gst_udp_port);
	snprintf(s_bitrate,128,"%d",target_bitrate);


	while (1>0) {
		//fork and run gst-send
		int pfd_to_child[2];
		int pfd_from_child[2];
		if (pipe(pfd_to_child)==-1) {
			fprintf(stderr,"failed to open pipe to child");
			exit(1);
		}
		if (pipe(pfd_from_child)==-1) {
			fprintf(stderr,"failed to open pipe to child");
			exit(1);
		}
		int pid=fork();
		if (pid==0) {
			close(pfd_to_child[1]); //close the write end
			close(pfd_from_child[0]); //close the read end
			dup2(pfd_to_child[0],0); // stdin from read pipe of to child
			dup2(pfd_from_child[1],1); //stdout to write pipe of from child
			//child
			fprintf(stderr, "passing in %s x %s\n",s_xres,s_yres);
			char * args[] = { "/home/pi/petbot/gst-send", s_xres,s_yres,gst_server_ip,s_udp_port, s_bitrate, NULL };
			int r = execv(args[0],args);
			fprintf(stderr,"SHOULD NEVER REACH HERE %d\n",r);
		}
		close(pfd_to_child[0]); //close the write end
		close(pfd_from_child[1]); //close the write end

		int code=0;
		//wait for wri
		fd_set rfds;
		struct timeval tv;
		int retval;

		//read from child process until something happens
		while (1>0) {
			tv.tv_sec = 10;
			tv.tv_usec = 0;
			FD_ZERO(&rfds);
			FD_SET(pfd_from_child[0], &rfds);
			FD_SET(pipe_to_gst[0], &rfds);
			retval = select(MAX(pfd_from_child[0],pipe_to_gst[0]) + 1, &rfds, NULL, NULL, &tv);
			//fprintf(stderr,"retval x is %d\n",retval);
			if (FD_ISSET(pfd_from_child[0], &rfds)) {
				//fprintf(stderr,"gst_client->read from gst-send\n");
				//should probably exit or do something based on code
				read(pfd_from_child[0],&code,sizeof(int)); //TODO shoudl check ret val
				if (code==GST_BYTES_SENT) {
					//bytes_sent=0;
					read(pfd_from_child[0],&bytes_sent,sizeof(guint64)); //TODO shoudl check ret val
					//fprintf(stderr,"BYTES SENT GOT FROM GST_SEND %"G_GUINT64_FORMAT"\n",bytes_sent);
				} else {
					fprintf(stderr,"gst_client->gst-send sends code %d\n",code);
					break;
				}
			} else if (FD_ISSET(pipe_to_gst[0], &rfds)) {
				//fprintf(stderr,"gst_client->read from manager\n");
				read(pipe_to_gst[0],&code,sizeof(int)); //TODO should check ret val
				//need to kill child
				int send_back=KILL_GST;
				write(pfd_to_child[1],&send_back,sizeof(int));
			} else if (retval<=0) {
				//TODO
			} else {
				//fprintf(stderr,"gst_client->select_timeout\n");
			}
		}

		if (tcp_status==0 || retries==0) {
			int send_back=GST_DIED;
			write(pipe_from_gst[1],&send_back,sizeof(int));
			break;
		}

		//figure out if we should try to restart and if so how
		if (code & GST_DIED) {
			fprintf(stderr, "SHOULD WE RESTART\n");
			reload_uvc();
			retries--;
		}

	}

	//master
	return NULL;
}



void * tcp_client(void * not_used) {
	struct sockaddr_in servaddr;
	char recvline[1000];

	sockfd=socket(AF_INET,SOCK_STREAM,0);

	bzero(&servaddr,sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr=inet_addr(tcp_server_ip);
	servaddr.sin_port=htons(tcp_server_port);

	connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr));


	struct timeval tv;
	tv.tv_sec = 10;  /* 10 Secs Timeout */
	tv.tv_usec = 0;  // Not init'ing this can cause strange errors
	setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,sizeof(struct timeval));

	//Get udp port
	int r = recvfrom(sockfd,recvline,9,0,NULL,NULL);
	recvline[r]='\0';
	gst_udp_port = atoi(recvline);
	//Get the xres
	r = recvfrom(sockfd,recvline,9,0,NULL,NULL);
	recvline[r]='\0';
	gst_xres = atoi(recvline);
	//Get the yres
	r = recvfrom(sockfd,recvline,9,0,NULL,NULL);
	recvline[r]='\0';
	gst_yres = atoi(recvline);

	fprintf(stderr, "Got UDP %d xres %d yres %d\n",gst_udp_port,gst_xres,gst_yres);

	sem_post(&tcp_client_mutex);

	char * pong = "pong";
	char * byte = "byte";
        fd_set rfds;
	//int retval;
	while (1>0) {
		struct timeval tv2;
		tv2.tv_sec = 10;  /* 10 Secs Timeout */
		tv2.tv_usec = 0;  // Not init'ing this can cause strange errors
		//fprintf(stderr,"TCP LOOP\n");
		FD_ZERO(&rfds);
		FD_SET(pipe_to_tcp[0], &rfds);
		FD_SET(sockfd, &rfds);
		select(MAX(sockfd,pipe_to_tcp[0]) + 1, &rfds, NULL, NULL, &tv2); //TODO
		if (FD_ISSET(pipe_to_tcp[0],&rfds)) {
			//got signal from master
			int code=0;
			read(pipe_to_tcp[0],&code,sizeof(int)); //TODO check retval
			assert(code==KILL_TCP);
			//timeout, kill TCP ?
			fprintf(stderr, "tcp got kill\n");
			close(sockfd);
			int send_back=TCP_DIED;;
			write(pipe_from_tcp[1],&send_back,sizeof(int));
			return NULL;
		} else if (FD_ISSET(sockfd, &rfds)) {
	     		r = recvfrom(sockfd,recvline,4,0,NULL,NULL);
	      		if (r!=4) {
				fprintf(stderr, "got something other then ping\n");
				close(sockfd);
				int send_back=TCP_DIED;;
				write(pipe_from_tcp[1],&send_back,sizeof(int));
				return NULL;
	      		}
			if (bytes_sent!=0) {
				//fprintf(stderr,"sending byte!\n");
	      			sendto(sockfd,byte,strlen(byte),0,
		     			(struct sockaddr *)&servaddr,sizeof(servaddr));
	      			sendto(sockfd,&bytes_sent,sizeof(guint64),0,
		     			(struct sockaddr *)&servaddr,sizeof(servaddr));
				bytes_sent=0;
			} else {
				//fprintf(stderr,"sending pong!\n");
	      			sendto(sockfd,pong,strlen(pong),0,
		     			(struct sockaddr *)&servaddr,sizeof(servaddr));
			}
			sleep(1);
		} else {
			//timeout, kill TCP ?
			fprintf(stderr, "tcp timeout\n");
			close(sockfd);
			int send_back=TCP_DIED;;
			write(pipe_from_tcp[1],&send_back,sizeof(int));
			return NULL;
		}
   }
}


void monitor() {
	tcp_status=1;
	gst_status=1;
	//start listening to news from chil
	fd_set rfds;
	struct timeval tv;
	//int retval;
	while (1>0) {
		tv.tv_sec = 10;
		tv.tv_usec = 0;
		FD_ZERO(&rfds);
		FD_SET(pipe_from_gst[0], &rfds);
		FD_SET(pipe_from_tcp[0], &rfds);
		select(MAX(pipe_from_gst[0],pipe_from_tcp[0]) + 1, &rfds, NULL, NULL, &tv);
		//fprintf(stderr,"retval y is %d\n",retval);

		int code=0;
		if (FD_ISSET(pipe_from_gst[0], &rfds)) {
			fprintf(stderr,"READING FROM GST!\n");
			int ret=read(pipe_from_gst[0],&code,sizeof(int));
			assert(ret==sizeof(int) && code==GST_DIED);
			gst_status=0;
			if (tcp_status==1) {
				int send_back=KILL_TCP;
				fprintf(stderr,"SENDING KILL TCP\n");
				int wrote = write(pipe_to_tcp[1],&send_back,sizeof(int));
				if (wrote!=sizeof(int)) {
					fprintf(stderr,"Failed to write to TCP\n");
				}
			} else {
				fprintf(stderr,"gst-manager->gst is dead\n");
			}
		} else if (FD_ISSET(pipe_from_tcp[0],&rfds)) {
			fprintf(stderr,"READING FROM TCP!\n");
			int ret=read(pipe_from_tcp[0],&code,sizeof(int));
			tcp_status=0;
			assert(ret==sizeof(int) && code==TCP_DIED);
			if (gst_status==1) {
				int send_back=KILL_GST;
				write(pipe_to_gst[1],&send_back,sizeof(int));
			} else {
				fprintf(stderr,"gst-manager->tcp is dead\n");
			}
		} else {
			//fprintf(stderr,"MONITOR TIMEOUT\n");
		}
		


		if (gst_status==0 && tcp_status==0) {
			break;
		}
	}
	fprintf(stderr,"Exiting gst-manager\n");
	exit(1);
}


int main(int argc, char *argv[]) {
  if (argc!=3) {
	fprintf(stdout, "%s ip tcp_port\n",argv[0]);
	exit(1);
  }

	bytes_sent=0;
  gst_udp_port=0;
  tcp_server_ip=argv[1];
  gst_server_ip=tcp_server_ip; //TODO make variable from server?
  tcp_server_port=atoi(argv[2]);
  if (tcp_server_port<=0) {
	fprintf(stderr,"Failed to parse port\n");
	exit(1);
  }



  sem_init(&restart_mutex, 0, 0);
  sem_init(&tcp_client_mutex, 0, 0);
  if (pipe(pipe_to_gst)==-1) {
	fprintf(stderr,"Failed to init pipe to gst\n");
  }
  if (pipe(pipe_to_tcp)==-1) {
	fprintf(stderr,"Failed to init pipe to tcp\n");
  }
  if (pipe(pipe_from_gst)==-1) {
	fprintf(stderr,"Failed to init pipe from gst\n");
  }
  if (pipe(pipe_from_tcp)==-1) {
	fprintf(stderr,"Failed to init pipe from tcp\n");
  }

  pthread_t tcp_thread, gst_thread;
  int  iret1, iret2;

  //start the tcp_client
  iret1 = pthread_create( &tcp_thread, NULL, tcp_client, NULL);
  if(iret1) {
    fprintf(stderr,"Error - pthread_create() return code: %d\n",iret1);
    exit(EXIT_FAILURE);
  }
  //start gst client
  iret2 = pthread_create( &gst_thread, NULL, gst_client, NULL);
  if(iret2) {
    fprintf(stderr,"Error - pthread_create() return code: %d\n",iret2);
    exit(EXIT_FAILURE);
  }
  //wait for gst client



  monitor();

  return 0;
}
