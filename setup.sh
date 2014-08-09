#!/bin/bash
/usr/local/bin/gpio mode 2 out
/usr/local/bin/gpio mode 3 out
/usr/local/bin/gpio mode 4 out
/usr/local/bin/gpio mode 11 out
/usr/local/bin/gpio mode 7 out
/usr/local/bin/gpio mode 6 out
/usr/local/bin/gpio write 7 1
/usr/local/bin/gpio write 6 1
/usr/local/bin/gpio load spi
/usr/local/bin/gpio mode 11 out
