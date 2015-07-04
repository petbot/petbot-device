#!/usr/bin/python
 
import spidev
import time
 
spi = spidev.SpiDev()
spi.open(0,0)
 
def read_spi(channel):
    spidata = spi.xfer2([96,0])
    print("Raw ADC:      {}".format(spidata))
    data = ((spidata[0] & 3) << 8) + spidata[1]
    return data
 
try:
    while True:
        channeldata = read_spi(0)
        voltage = round(((channeldata * 3300) / 1024),0)
        temperature = ((voltage - 500) / 10)
        print("Data (dec)    {}".format(channeldata))
        print("Data (bin)    {:010b}".format(channeldata))
        print("Voltage (mV): {}".format(voltage))
        print("-----------------")
        time.sleep(1)
 
except KeyboardInterrupt:
    spi.close()
