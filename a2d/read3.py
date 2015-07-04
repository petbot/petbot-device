import spidev

DEBUG = 0
 
spi = spidev.SpiDev()
spi.open(0,0)
 
# read SPI data from MCP3002 chip
def get_adc(channel):
        # Only 2 channels 0 and 1 else return -1
        if ((channel > 1) or (channel < 0)):
                return -1
       
        # Send start bit, sgl/diff, odd/sign, MSBF
        # channel = 0 sends 0000 0001 1000 0000 0000 0000
        # channel = 1 sends 0000 0001 1100 0000 0000 0000
        # sgl/diff = 1; odd/sign = channel; MSBF = 0
        r = spi.xfer2([1,(2+channel)<<6,0])
       
        # spi.xfer2 returns same number of 8 bit bytes
        # as sent. In this case, 3 - 8 bit bytes are returned
        # We must then parse out the correct 10 bit byte from
        # the 24 bits returned. The following line discards
        # all bits but the 10 data bits from the center of
        # the last 2 bytes: XXXX XXXX - XXXX DDDD - DDDD DDXX
        ret = ((r[1]&31) << 6) + (r[2] >> 2)
        return ret     
 
print get_adc(0)
print get_adc(1)
