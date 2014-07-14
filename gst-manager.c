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

void reload_uvc() {
	int pid=fork();
	if (pid==0) {
		//child
		char * args[] = { "/sbin/modprobe", "-r", "uvcvideo", NULL };
		int r = execv(args[0],args);
		fprintf(stderr,"SHOULD NEVER REACH HERE %d\n",r);
	}
	//master
	wait(NULL);
	fprintf(stderr,"Unloaded UVC\n");
	pid=fork();
	if (pid==0) {
		//child
		char * args[] = { "/sbin/modprobe", "uvcvideo", NULL };
		int r = execv(args[0],args);
		fprintf(stderr,"SHOULD NEVER REACH HERE %d\n",r);
	}
	//master
	wait(NULL);
	fprintf(stderr,"Loaded UVC\n");
	return;
}

void * gst_client(void * not_used ) { //(char * ip, int udp_port, int target_bitrate, int height, int width) {
  if (retries%4==1) {
  	reload_uvc();
  }
  GstCaps *capture_caps, *converted_caps,*h264_caps;

  GstElement *v4l2src, *videorate, *queue, *videoconvert, *omxh264enc, *rtph264pay, *udpsink;
  GstBus *bus;
  GstMessage *msg;
  GstStateChangeReturn ret;
  
  /* Initialize GStreamer */
  gst_init (NULL,NULL);
   
  /* Create the elements */
  //sprintf(buffer,"v4l2src ! video/x-raw, width=%d, height=%d, framerate=30/1 ! videorate !  video/x-raw,framerate=15/1 ! queue ! videoconvert ! omxh264enc target-bitrate=%d control-rate=variable ! rtph264pay pt=96 config-interval=3 ! udpsink host=%s port=%d", xres, yres, target_bitrate, ip, udp_port); 
  v4l2src = gst_element_factory_make ("v4l2src", "camsrc");
  videorate = gst_element_factory_make ("videorate", "videorate");
  queue = gst_element_factory_make ("queue","queue");
  videoconvert = gst_element_factory_make ("videoconvert","videoconvert");
  omxh264enc = gst_element_factory_make ("omxh264enc","omxh264enc");
  rtph264pay = gst_element_factory_make ("rtph264pay", "rtph264pay");
  udpsink = gst_element_factory_make ("udpsink", "udpsink");

  /* Create the caps filters */
  capture_caps = gst_caps_new_simple("video/x-raw","width", G_TYPE_INT, gst_xres, "height", G_TYPE_INT, gst_yres, "framerate", GST_TYPE_FRACTION, 30, 1, NULL);
  converted_caps = gst_caps_new_simple("video/x-raw","width", G_TYPE_INT, gst_xres, "height", G_TYPE_INT, gst_yres, "framerate", GST_TYPE_FRACTION, 15, 1, NULL);
  h264_caps = gst_caps_new_simple("video/x-h264","profile", G_TYPE_STRING,"baseline",NULL);


  /* Create the empty pipeline */
  pipeline = gst_pipeline_new ("send-pipeline");
   
  if (!pipeline || !v4l2src || !videorate || !queue || !videoconvert || !omxh264enc || !rtph264pay || !udpsink) {
    g_printerr ("Not all elements could be created.\n");
    return NULL;
  }
  
  /* Build the pipeline */
  gst_bin_add_many (GST_BIN (pipeline), v4l2src, videorate, queue, videoconvert, omxh264enc, rtph264pay, udpsink,  NULL);
  fprintf(stderr, "Linking pipeline..\n");
  /* Link the eleemnts */
  if (gst_element_link_filtered(v4l2src , videorate, capture_caps) !=TRUE ) {
    g_printerr ("Could not connect v4l2src to videorate.\n");
    gst_object_unref (pipeline);
    pipeline=NULL;
    sem_post(&gst_client_mutex);
    sem_post(&restart_mutex);
    return NULL;
  } 
  if (gst_element_link_filtered(videorate , queue, converted_caps) !=TRUE ) {
    g_printerr ("Could not connect videorate to queue.\n");
    gst_object_unref (pipeline);
    pipeline=NULL;
    sem_post(&gst_client_mutex);
    sem_post(&restart_mutex);
    return NULL;
  } 
  if (gst_element_link(queue , videoconvert ) !=TRUE ) {
    g_printerr ("Could not connect queue to videoconvert.\n");
    gst_object_unref (pipeline);
    pipeline=NULL;
    sem_post(&gst_client_mutex);
    sem_post(&restart_mutex);
    return NULL;
  } 
  if (gst_element_link(videoconvert , omxh264enc ) !=TRUE ) {
    g_printerr ("Could not connect videoconvert to omxh264enc.\n");
    gst_object_unref (pipeline);
    pipeline=NULL;
    sem_post(&gst_client_mutex);
    sem_post(&restart_mutex);
    return NULL;
  } 
  //if (gst_element_link(omxh264enc , rtph264pay ) !=TRUE ) {
  if (gst_element_link_filtered(omxh264enc , rtph264pay, h264_caps ) !=TRUE ) {
    g_printerr ("Could not connect omxh264enc to rtph264pay.\n");
    gst_object_unref (pipeline);
    pipeline=NULL;
    sem_post(&gst_client_mutex);
    sem_post(&restart_mutex);
    return NULL;
  } 
  if (gst_element_link(rtph264pay , udpsink ) !=TRUE ) {
    g_printerr ("Could not connect rpth264pay to udpsink.\n");
    gst_object_unref (pipeline);
    pipeline=NULL;
    sem_post(&gst_client_mutex);
    sem_post(&restart_mutex);
    return NULL;
  } 



  //make sure TCP client did its work
  if (gst_udp_port==0) {
  	sem_wait(&tcp_client_mutex);
  }
	
  /* set properties */
  //gst_base_src_set_live (GST_BASE_SRC (v4l2src), TRUE);
  g_object_set( G_OBJECT(v4l2src) , "do-timestamp", TRUE, "io-mode",0,NULL);
  //g_object_set( G_OBJECT(omxh264enc), "target-bitrate", target_bitrate, "control-rate", OMX_Video_ControlRateVariableSkipFrames, NULL);
  g_object_set( G_OBJECT(omxh264enc), "target-bitrate", target_bitrate, "control-rate", OMX_Video_ControlRateVariable, NULL);
  //g_object_set( G_OBJECT(omxh264enc), "target-bitrate", target_bitrate, "control-rate", OMX_Video_ControlRateConstantSkipFrames, NULL);
  g_object_set( G_OBJECT(rtph264pay), "pt", 96, "config-interval",1,NULL);
  g_object_set( G_OBJECT(udpsink), "host", gst_server_ip, "port", gst_udp_port, "sync",FALSE, "async", FALSE, NULL);
  g_object_set( G_OBJECT(queue), "max-size-buffers", 0, NULL);
  g_object_set( G_OBJECT(videorate), "drop-only", TRUE, NULL);  
 
  /* Start playing */
  ret = gst_element_set_state (pipeline, GST_STATE_PLAYING);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    g_printerr ("Unable to set the pipeline to the playing state.\n");
    gst_object_unref (pipeline);
    pipeline=NULL;
    sem_post(&gst_client_mutex);
    sem_post(&restart_mutex);
    return NULL;
  } else if (ret==GST_STATE_CHANGE_ASYNC) {
	fprintf(stderr,"STILL ASYNC\n");
  } else if (ret==GST_STATE_CHANGE_SUCCESS) {
        fprintf(stderr,"CHANGE SUCCESS\n");
  }
  
  fprintf(stderr, "Started pipeline\n");

  sem_post(&gst_client_mutex);
  /* Wait until error or EOS */
  bus = gst_element_get_bus (pipeline);
   
  /* Parse message */
  int go_on=4;
  int stream_status_messages=0;
  while (go_on>0) {
  	msg = gst_bus_timed_pop_filtered (bus, GST_SECOND/2, GST_MESSAGE_ANY | GST_MESSAGE_STREAM_START | GST_MESSAGE_ASYNC_DONE | GST_MESSAGE_ERROR | GST_MESSAGE_EOS);
	  if (msg != NULL) {
	    //fprintf(stderr, "Got a messsage , %d , %s\n", GST_MESSAGE_TYPE (msg), GST_MESSAGE_TYPE_NAME(msg));
	    GError *err;
	    gchar *debug_info;
	    
	    switch (GST_MESSAGE_TYPE (msg)) {
	      case GST_MESSAGE_ERROR:
		gst_message_parse_error (msg, &err, &debug_info);
		g_printerr ("Error received from element %s: %s\n", GST_OBJECT_NAME (msg->src), err->message);
		g_printerr ("Debugging information: %s\n", debug_info ? debug_info : "none");
		g_clear_error (&err);
		g_free (debug_info);
		go_on=0;
		break;
	      case GST_MESSAGE_EOS:
		g_print ("End-Of-Stream reached.\n");
		go_on=0;
		break;
	      case GST_MESSAGE_STREAM_START:
	      case GST_MESSAGE_ASYNC_DONE:
	        g_print("ASYNC DONE\n");
		break;
	      case GST_MESSAGE_STATE_CHANGED:
		{
		GstState old_state, new_state, pending_state;
    		gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);
		fprintf(stderr,"state changed , %s , %s , %s \n",gst_element_state_get_name(old_state),gst_element_state_get_name(new_state), gst_element_state_get_name(pending_state));
		}
		break;
	      case GST_MESSAGE_STREAM_STATUS:
		{
		GstStreamStatusType message_type;
      		gst_message_parse_stream_status(msg, &message_type, NULL);
      		fprintf(stderr,"Stream status: %d\n", message_type);
		stream_status_messages++;
		}
		break;
	      default:
		//fprintf(stderr,"GOT UNEXPECTED MESSAGE\n");
		fprintf(stderr,"%s\n",GST_MESSAGE_TYPE_NAME(msg));
		break;
	    }
	    gst_message_unref (msg);
	  } else {
		if (shutdown_now==1) {
			break;
		}
		if (stream_status_messages<6) {
			go_on-=1;
		}
	} 
  }
 
  fprintf(stderr, "Cleaning up\n");
 
  /* Free resources */
  gst_object_unref (bus);
  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (pipeline);
  pipeline=NULL;
	
  if (retries==0 || shutdown_now==1) {
  	sem_post(&restart_mutex);
  	return NULL;
  }
  retries-=1;
  fprintf(stderr,"Retrying internally %d\n",retries);
  return gst_client(NULL); //try again
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
