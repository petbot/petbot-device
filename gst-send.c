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


int run(char * ip, int udp_port, int target_bitrate, int height, int width) {

  GstCaps *capture_caps, *converted_caps,*h264_caps;

  GstElement *pipeline, *v4l2src, *videorate, *queue, *videoconvert, *omxh264enc, *rtph264pay, *udpsink;
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
  capture_caps = gst_caps_new_simple("video/x-raw","width", G_TYPE_INT, 640, "height", G_TYPE_INT, 480, "framerate", GST_TYPE_FRACTION, 30, 1, NULL);
  converted_caps = gst_caps_new_simple("video/x-raw","width", G_TYPE_INT, 640, "height", G_TYPE_INT, 480, "framerate", GST_TYPE_FRACTION, 15, 1, NULL);
  h264_caps = gst_caps_new_simple("video/x-h264","profile", G_TYPE_STRING,"baseline",NULL);


  /* Create the empty pipeline */
  pipeline = gst_pipeline_new ("send-pipeline");
   
  if (!pipeline || !v4l2src || !videorate || !queue || !videoconvert || !omxh264enc || !rtph264pay || !udpsink) {
    g_printerr ("Not all elements could be created.\n");
    return -1;
  }
  
  /* Build the pipeline */
  gst_bin_add_many (GST_BIN (pipeline), v4l2src, videorate, queue, videoconvert, omxh264enc, rtph264pay, udpsink,  NULL);
  fprintf(stderr, "Linking pipeline..\n");
  /* Link the eleemnts */
  if (gst_element_link_filtered(v4l2src , videorate, capture_caps) !=TRUE ) {
    g_printerr ("Could not connect v4l2src to videorate.\n");
    gst_object_unref (pipeline);
    return -1;
  } 
  if (gst_element_link_filtered(videorate , queue, converted_caps) !=TRUE ) {
    g_printerr ("Could not connect videorate to queue.\n");
    gst_object_unref (pipeline);
    return -1;
  } 
  if (gst_element_link(queue , videoconvert ) !=TRUE ) {
    g_printerr ("Could not connect queue to videoconvert.\n");
    gst_object_unref (pipeline);
    return -1;
  } 
  if (gst_element_link(videoconvert , omxh264enc ) !=TRUE ) {
    g_printerr ("Could not connect videoconvert to omxh264enc.\n");
    gst_object_unref (pipeline);
    return -1;
  } 
  //if (gst_element_link(omxh264enc , rtph264pay ) !=TRUE ) {
  if (gst_element_link_filtered(omxh264enc , rtph264pay, h264_caps ) !=TRUE ) {
    g_printerr ("Could not connect omxh264enc to rtph264pay.\n");
    gst_object_unref (pipeline);
    return -1;
  } 
  if (gst_element_link(rtph264pay , udpsink ) !=TRUE ) {
    g_printerr ("Could not connect rpth264pay to udpsink.\n");
    gst_object_unref (pipeline);
    return -1;
  } 

  /* set properties */
  g_object_set( G_OBJECT(v4l2src) , "do-timestamp", TRUE, NULL);
  g_object_set( G_OBJECT(omxh264enc), "target-bitrate", target_bitrate, "control-rate", OMX_Video_ControlRateVariableSkipFrames, NULL);
  //g_object_set( G_OBJECT(omxh264enc), "target-bitrate", target_bitrate, "control-rate", OMX_Video_ControlRateConstantSkipFrames, NULL);
  g_object_set( G_OBJECT(rtph264pay), "pt", 96, "config-interval",1,NULL);
  g_object_set( G_OBJECT(udpsink), "host", ip, "port", udp_port, "sync",FALSE, "async", FALSE, NULL);
  g_object_set( G_OBJECT(queue), "max-size-buffers", 0, NULL);
  g_object_set( G_OBJECT(videorate), "drop-only", TRUE, NULL);  
 
  /* Start playing */
  ret = gst_element_set_state (pipeline, GST_STATE_PLAYING);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    g_printerr ("Unable to set the pipeline to the playing state.\n");
    gst_object_unref (pipeline);
    return -1;
  }
  
  fprintf(stderr, "Started pipeline\n");

  /* Wait until error or EOS */
  bus = gst_element_get_bus (pipeline);
   
  /* Parse message */
  int go_on=2;
  int got_async=0;
  while (go_on>0) {
  	msg = gst_bus_timed_pop_filtered (bus, GST_SECOND, GST_MESSAGE_ANY | GST_MESSAGE_STREAM_START | GST_MESSAGE_ASYNC_DONE | GST_MESSAGE_ERROR | GST_MESSAGE_EOS);
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
		got_async=1;
		break;
	      default:
		/* We should not reach here because we only asked for ERRORs and EOS */
		//g_printerr ("Unexpected message received.\n");
		//break;
		break;
	    }
	    gst_message_unref (msg);
	  } else {
		if (got_async==0) {
			go_on-=1;
		}
	} 
  }
 
  fprintf(stderr, "Cleaning up\n");
 
  /* Free resources */
  gst_object_unref (bus);
  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (pipeline);
  return 0;
}

int main(int argc, char *argv[]) {
  if (argc!=6) {
    fprintf(stdout, "%s Xres Yres ip udp_port target_bitrate[try 500000]\n",argv[0]);
    exit(1);
  }

  int xres=atoi(argv[1]);
  int yres=atoi(argv[2]);

  if (xres==320) {
	assert(yres==240);
  } else if (xres==640) {
	assert(yres==480);
  } else {
	printf("unsupported resoltuions. supported is 320x240 and 640x480\n");
	exit(1);
  }

  char * ip=argv[3];
  int udp_port=atoi(argv[4]);
  int target_bitrate=atoi(argv[5]);


  int tries=5;
  int i;
  for (i=0; i<tries; i++) {
  	run(ip,udp_port,target_bitrate,xres,yres);
  }	
  
  return 0;
}
