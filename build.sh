#gcc `pkg-config gstreamer-1.0 --cflags` -I/opt/vc/include/IL/ gst-send.c -o gst-send `pkg-config gstreamer-1.0 --libs` -O3 -g -Wall
gcc `pkg-config gstreamer-1.0 --cflags` -I/opt/vc/include/IL/ gst-manager.c -o gst-manager `pkg-config gstreamer-1.0 --libs` -O3 -Wall
gcc -lm -lwiringPi ./single_cookie/single_cookie_gear.c -o ./single_cookie/single_cookie
