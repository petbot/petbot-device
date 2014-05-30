#!/bin/bash

gst-launch-1.0 -v v4l2src ! 'video/x-raw, width=320, height=240, framerate=30/1' ! queue ! videoconvert ! omxh264enc ! rtph264pay pt=96 ! udpsink host=162.243.126.214 port=500

