#!/bin/bash
sudo gpio write 7 0
/usr/bin/mpg123 $1
sudo gpio write 7 1
