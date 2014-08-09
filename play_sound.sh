#!/bin/bash
sudo gpio write 7 0
/usr/bin/curl "$1" | /usr/bin/mpg123 -
sudo gpio write 7 1
