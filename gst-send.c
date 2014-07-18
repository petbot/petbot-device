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

#include <pthread.h> 
#include <semaphore.h>

#include <unistd.h>

char * ip;
int xres,yres,udp_port,target_bitrate;
int shutdown_now=0;
sem_t gst_off;

int pipefd[2];

void * ret_run() {
	fprintf(stderr,"Shutting down gst...\n");
	sem_post(&gst_off);
	fprintf(stderr,"Shutting down gst...\n");
	fprintf(stdin,"POOP\n");
	//close(0);
	write(pipefd[1],"CLOSE",6);
	fprintf(stderr,"Shutting down gst...\n");
	return NULL;
}

void * run(void * v) {
  GstCaps *capture_caps, *converted_caps,*h264_caps;

  GstElement *pipeline, *v4l2src, *videorate, *queue, *videoconvert, *omxh264enc, *rtph264pay, *udpsink;
  GstBus *bus;
  GstMessage *msg;
  GstStateChangeReturn ret;
  /* Initialize GStreamer */
  gst_init (NULL,NULL);
   
  /* Create the elements */
  //sprintf(buffer,"v4l2src ! video/x-raw, xres=%d, yres=%d, framerate=30/1 ! videorate !  video/x-raw,framerate=15/1 ! queue ! videoconvert ! omxh264enc target-bitrate=%d control-rate=variable ! rtph264pay pt=96 config-interval=3 ! udpsink host=%s port=%d", xres, yres, target_bitrate, ip, udp_port); 
  v4l2src = gst_element_factory_make ("v4l2src", "camsrc");
  videorate = gst_element_factory_make ("videorate", "videorate");
  queue = gst_element_factory_make ("queue","queue");
  videoconvert = gst_element_factory_make ("videoconvert","videoconvert");
  omxh264enc = gst_element_factory_make ("omxh264enc","omxh264enc");
  rtph264pay = gst_element_factory_make ("rtph264pay", "rtph264pay");
  udpsink = gst_element_factory_make ("udpsink", "udpsink");

  /* Create the caps filters */
  capture_caps = gst_caps_new_simple("video/x-raw","xres", G_TYPE_INT, xres, "yres", G_TYPE_INT, yres, "framerate", GST_TYPE_FRACTION, 30, 1, NULL);
  converted_caps = gst_caps_new_simple("video/x-raw","xres", G_TYPE_INT, xres, "yres", G_TYPE_INT, yres, "framerate", GST_TYPE_FRACTION, 15, 1, NULL);
  h264_caps = gst_caps_new_simple("video/x-h264","profile", G_TYPE_STRING,"baseline",NULL);


  /* Create the empty pipeline */
  pipeline = gst_pipeline_new ("send-pipeline");
   
  if (!pipeline || !v4l2src || !videorate || !queue || !videoconvert || !omxh264enc || !rtph264pay || !udpsink) {
    g_printerr ("Not all elements could be created.\n");
    return ret_run();
  }
  
  /* Build the pipeline */
  gst_bin_add_many (GST_BIN (pipeline), v4l2src, videorate, queue, videoconvert, omxh264enc, rtph264pay, udpsink,  NULL);
  fprintf(stderr, "Linking pipeline..\n");
  /* Link the eleemnts */
  if (gst_element_link_filtered(v4l2src , videorate, capture_caps) !=TRUE ) {
    g_printerr ("Could not connect v4l2src to videorate.\n");
    gst_object_unref (pipeline);
    return ret_run();
  } 
  if (gst_element_link_filtered(videorate , queue, converted_caps) !=TRUE ) {
    g_printerr ("Could not connect videorate to queue.\n");
    gst_object_unref (pipeline);
    return ret_run();
  } 
  if (gst_element_link(queue , videoconvert ) !=TRUE ) {
    g_printerr ("Could not connect queue to videoconvert.\n");
    gst_object_unref (pipeline);
    return ret_run();
  } 
  if (gst_element_link(videoconvert , omxh264enc ) !=TRUE ) {
    g_printerr ("Could not connect videoconvert to omxh264enc.\n");
    gst_object_unref (pipeline);
    return ret_run();
  } 
  //if (gst_element_link(omxh264enc , rtph264pay ) !=TRUE ) {
  if (gst_element_link_filtered(omxh264enc , rtph264pay, h264_caps ) !=TRUE ) {
    g_printerr ("Could not connect omxh264enc to rtph264pay.\n");
    gst_object_unref (pipeline);
    return ret_run();
  } 
  if (gst_element_link(rtph264pay , udpsink ) !=TRUE ) {
    g_printerr ("Could not connect rpth264pay to udpsink.\n");
    gst_object_unref (pipeline);
    return ret_run();
  } 

  /* set properties */
  //gst_base_src_set_live (GST_BASE_SRC (v4l2src), TRUE);
  g_object_set( G_OBJECT(v4l2src) , "do-timestamp", TRUE, "io-mode",1,NULL);
  //g_object_set( G_OBJECT(omxh264enc), "target-bitrate", target_bitrate, "control-rate", OMX_Video_ControlRateVariableSkipFrames, NULL);
  g_object_set( G_OBJECT(omxh264enc), "target-bitrate", target_bitrate, "control-rate", OMX_Video_ControlRateVariable, NULL);
  //g_object_set( G_OBJECT(omxh264enc), "target-bitrate", target_bitrate, "control-rate", OMX_Video_ControlRateConstantSkipFrames, NULL);
  g_object_set( G_OBJECT(rtph264pay), "pt", 96, "config-interval",1,NULL);
  fprintf(stderr, "IP %s udp_port %d target_bit %d\n",ip,udp_port,target_bitrate);
  g_object_set( G_OBJECT(udpsink), "host", ip, "port", udp_port, "sync",FALSE, "async", FALSE, NULL);
  g_object_set( G_OBJECT(queue), "max-size-buffers", 0, NULL);
  g_object_set( G_OBJECT(videorate), "drop-only", TRUE, NULL);  
 
  /* Start playing */
  ret = gst_element_set_state (pipeline, GST_STATE_PLAYING);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    g_printerr ("Unable to set the pipeline to the playing state.\n");
    gst_element_set_state (pipeline, GST_STATE_NULL);
    gst_object_unref (pipeline);
    return ret_run();
  } else if (ret==GST_STATE_CHANGE_ASYNC) {
	fprintf(stderr,"STILL ASYNC\n");
  } else if (ret==GST_STATE_CHANGE_SUCCESS) {
        fprintf(stderr,"CHANGE SUCCESS\n");
  }
  
  fprintf(stderr, "Started pipeline\n");

  /* Wait until error or EOS */
  bus = gst_element_get_bus (pipeline);

  guint64 last_in=0, last_out=0, last_dropped=0, last_duplicated=0;
  int i=0;   
   
  /* Parse message */
  int go_on=4;
  int stream_status_messages=0;
  while (go_on>=0) {
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
		go_on=-1;
		break;
	      case GST_MESSAGE_EOS:
		g_print ("End-Of-Stream reached.\n");
		go_on=-1;
		break;
	      case GST_MESSAGE_STREAM_START:
	      case GST_MESSAGE_ASYNC_DONE:
	        g_print("ASYNC DONE\n");
		break;
	      case GST_MESSAGE_STATE_CHANGED:
		{
		GstState old_state, new_state, pending_state;
    		gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);
		fprintf(stderr,"state change , %s , %s , %s \n",gst_element_state_get_name(old_state),gst_element_state_get_name(new_state), gst_element_state_get_name(pending_state));
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
		/* We should not reach here because we only asked for ERRORs and EOS */
		//g_printerr ("Unexpected message received.\n");
		//break;
		fprintf(stderr,"%s\n",GST_MESSAGE_TYPE_NAME(msg));
		break;
	    }
	    gst_message_unref (msg);
	  } else {
		fprintf(stderr,"TIMEOUT!\n");
		if (shutdown_now==1) {
			break;
		}
		if (stream_status_messages<6) {
			if (go_on==0) {
				return ret_run();
			}
			go_on-=1;
		}
		if (i++==8) {
		//lets see how many frames have been processed
			//g_object_get (videorate, "in", &in, "out", &out, "drop", &dropped,
			//    "duplicate", &duplicated, NULL);
			guint64 out;
			g_object_get (videorate, "out", &out, NULL);
			if (out-last_out<10) {
				//fprintf(stderr,"%"G_GUINT64_FORMAT" %"G_GUINT64_FORMAT" %"G_GUINT64_FORMAT" %"G_GUINT64_FORMAT"\n",in,out,dropped,duplicated);
				fprintf(stderr,"Frames out are low! %"G_GUINT64_FORMAT"\n",out);
				gst_element_set_state (pipeline, GST_STATE_NULL);
				break;
			} else {
				//fprintf(stderr,"Frames out are %"G_GUINT64_FORMAT"\n",out);
			}
			last_out=out;
			i=0;
		}
		fprintf(stderr,"TIMEOUT2!\n");
	} 
  }
 
  fprintf(stderr, "Cleaning up\n");
 
  /* Free resources */
  gst_object_unref (bus);
  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (pipeline);
  return ret_run();
}

int main(int argc, char *argv[]) {
  if (argc!=6) {
    fprintf(stdout, "%s Xres Yres ip udp_port target_bitrate[try 500000]\n",argv[0]);
    exit(1);
  }

  xres=atoi(argv[1]);
  yres=atoi(argv[2]);

  if (xres==320) {
	assert(yres==240);
  } else if (xres==640) {
	assert(yres==480);
  } else {
	printf("unsupported resoltuions. supported is 320x240 and 640x480\n");
	fprintf(stderr,"resolutions give %d x %d\n",xres,yres);
	exit(1);
  }

  ip=argv[3];
  udp_port=atoi(argv[4]);
  target_bitrate=atoi(argv[5]);


  sem_init(&gst_off,0,0);

  pthread_t gst_thread;
  int  iret1;

  if (pipe(pipefd)==-1) {
	fprintf(stderr,"Failed to pipe\n");
	exit(1);
  }

  //start the tcp_client
  iret1 = pthread_create( &gst_thread, NULL, run, NULL);
  if(iret1) {
    fprintf(stderr,"Error - pthread_create() return code: %d\n",iret1);
    exit(EXIT_FAILURE);
  }

  char buffer[2048];
fd_set rfds;
struct timeval tv;
int retval; 
    tv.tv_sec = 10;
    tv.tv_usec = 0;


  while (1>0) {
    FD_ZERO(&rfds);
    FD_SET(0, &rfds);
    FD_SET(pipefd[0], &rfds);
    retval = select(pipefd[0] + 1, &rfds, NULL, NULL, &tv);
    fprintf(stderr,"retval is %d\n",retval);
    if (FD_ISSET(0, &rfds)) {
	fprintf(stderr,"HAVE STUFF TO READ!\n");
	  struct timespec ts;
	  clock_gettime(CLOCK_REALTIME, &ts);
	  ts.tv_sec += 2;
	  sem_timedwait(&gst_off, &ts);
	  pthread_kill(gst_thread,-9);
	return 0;
    } else if (FD_ISSET(pipefd[0], &rfds)) {
	fprintf(stderr,"CHILD WROTE ME\n");
	return 0;
    } else {
	fprintf(stderr,"GOT NOTHING!\n");
	//exit(1);
    } 
  }

  return 0; 
}
