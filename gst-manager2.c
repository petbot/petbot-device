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


sem_t tcp_client_mutex;
char * tcp_server_ip;
int tcp_server_port;
int sockfd;

sem_t gst_client_mutex;
char * gst_server_ip;
int gst_udp_port,gst_xres,gst_yres;
int target_bitrate=350000;
GstElement *pipeline;

sem_t restart_mutex;
int shutdown_now;

int retries=20;

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
	exit(1);
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
    	sem_post(&gst_client_mutex);
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
	//fork and run gst-send
	int pid=fork();
	if (pid==0) {
		//child
		fprintf(stderr, "passing in %s x %s\n",s_xres,s_yres);
		char * args[] = { "/home/pi/petbot/gst-send", s_xres,s_yres,gst_server_ip,s_udp_port, s_bitrate, NULL };
		int r = execv(args[0],args);
		fprintf(stderr,"SHOULD NEVER REACH HERE %d\n",r);
	}
	//master
	sem_post(&gst_client_mutex);
	fprintf(stderr,"GST_MANAGER_WAITING FOR GST_CLIENT\n");
	wait(NULL);
  	sem_wait(&restart_mutex);
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
   while (1) {
      //fprintf(stderr,"ping/pong\n");
      r = recvfrom(sockfd,recvline,4,0,NULL,NULL);
      if (r!=4) {
	fprintf(stderr, "got something other then ping\n");
	sem_post(&restart_mutex);
	return NULL;
      }
      sendto(sockfd,pong,strlen(pong),0,
             (struct sockaddr *)&servaddr,sizeof(servaddr));
   }
}

int main(int argc, char *argv[]) {
  if (argc!=3) {
	fprintf(stdout, "%s ip tcp_port\n",argv[0]);
	exit(1);
  }


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
  sem_wait(&gst_client_mutex);


  //wait for restart request
  sem_wait(&restart_mutex);
  fprintf(stderr,"GOT RESTART\n");
  shutdown_now=1;
  close(sockfd);
  sem_wait(&restart_mutex);
  fprintf(stderr,"GOT RESTART\n");

  return 0;
}
