#include <stdint.h>
#include <unistd.h>
#include <sys/time.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
//#include <linux/types.h>
#include <linux/spi/spidev.h>
//#include "spidev.h" 

//#define DEBUG 

char spi_dev[16];
long speed=50000;
long udelay=0;
struct spi_ioc_transfer xfer[2];
struct timeval start, finish;
long msec;
int file;

unsigned short crc16(unsigned char* data_p, unsigned char length){
    unsigned char x;
    unsigned short crc = 0xFFFF;

    while (length--){
        x = crc >> 8 ^ *data_p++;
        x ^= x>>4;
        crc = (crc << 8) ^ ((unsigned short)(x << 12)) ^
             ((unsigned short)(x <<5)) ^ ((unsigned short)x);
    }
    return crc;
}
long timevaldiff(struct timeval *starttime, struct timeval *finishtime)
{
    long msec;
    msec=(finishtime->tv_sec-starttime->tv_sec)*1000;
    msec+=(finishtime->tv_usec-starttime->tv_usec)/1000;
    return msec;
}
int spi_init(char filename[40])
{
    int file;
    __u8    mode, lsb, bits;
    __u32 speed=speed;

    if ((file = open(filename,O_RDWR)) < 0)
    {
        printf("Failed to open the bus.");
        /* ERROR HANDLING; you can check errno to see what went wrong */
        exit(1);
    }

    /*multiple modes:
      mode |= SPI_LOOP; 
      mode |= SPI_CPHA; 
      mode |= SPI_CPOL; 
      mode |= SPI_LSB_FIRST; 
      mode |= SPI_CS_HIGH; 
      mode |= SPI_3WIRE; 
      mode |= SPI_NO_CS; 
      mode |= SPI_READY;
      if (ioctl(file, SPI_IOC_WR_MODE, &mode)<0)       {
      perror("can't set spi mode");
      return;
      }
     */

    if (ioctl(file, SPI_IOC_RD_MODE, &mode) < 0)
    {
        perror("SPI rd_mode");
        return;
    }
    if (ioctl(file, SPI_IOC_RD_LSB_FIRST, &lsb) < 0)
    {
        perror("SPI rd_lsb_fist");
        return;
    }
    if (ioctl(file, SPI_IOC_RD_BITS_PER_WORD, &bits) < 0)
    {
        perror("SPI bits_per_word");
        return;
    }
    if (ioctl(file, SPI_IOC_RD_MAX_SPEED_HZ, &speed) < 0)
    {
        perror("SPI max_speed_hz");
        return;
    }


    printf("%s: spi mode %d, %d bits %sper word, %d Hz max\n",
        filename, mode, bits, lsb ? "(lsb first) " : "", speed);

    xfer[0].cs_change = 0; // Keep CS activated
    xfer[0].delay_usecs = udelay;
    xfer[0].speed_hz = speed;
    xfer[0].bits_per_word = 8; // 8-bites per word only

    xfer[1].cs_change = 0;
    xfer[1].delay_usecs = 0;
    xfer[1].speed_hz = speed;
    xfer[1].bits_per_word = 8;

    return file;
}

int spi_read(int add1,int add2,int nbytes,char* buffer,int file)
{
    static char cmd[3];
    int status;

    memset(buffer, 0, nbytes);
    cmd[0] = 0x01;
    cmd[1] = add1;
    cmd[2] = add2;
    xfer[0].tx_buf = (unsigned long)cmd;
    xfer[0].len = 3; /* Length of  command to write*/

    xfer[1].rx_buf = (unsigned long) buffer;
    xfer[1].len = nbytes; /* Length of Data to read */

    status = ioctl(file, SPI_IOC_MESSAGE(2), xfer);
    if (status < 0)
    {
        perror("SPI_IOC_MESSAGE");
    }
    return status;
}
int spi_slave_read(int nbytes,char* buffer, int file)
{
    int status;

    memset(buffer, 0, nbytes);
    xfer[0].rx_buf = (unsigned long) buffer;
    xfer[0].len = nbytes; /* Length of Data to read */
    status = ioctl(file, SPI_IOC_MESSAGE(1), xfer);
    if (status < 0)
    {
        perror("SPI_IOC_MESSAGE");
    }
    return status;
}

void spi_write(int nbytes,char* buffer,int file)
{
    int status;
    xfer[0].tx_buf = (unsigned long)buffer;
    xfer[0].len = nbytes; /* Length of command to write */
    status = ioctl(file, SPI_IOC_MESSAGE(1), xfer);
    if (status < 0)
    {
        perror("SPI_IOC_MESSAGE");
        return;
    }
}

void dump(char *buffer, int len){
    int x;
    for(x=0; x<len; x++){
        printf("0x%X ",*(buffer+x));
    }
    printf("\n");
}

void quit(int sig)
{
    printf("Closing...\n");
    close(file);
    exit(0);
}
main ( int argc, char **argv ) {
    int x, y, len,status;
    long frame, last_frame, good, bad;
    unsigned short crc;
    char *rd,*wr;

    signal (SIGINT, quit);
    if(argc!=4 && argc!=5){
#ifdef SLAVE
        printf("Usage: ./slave <dev> <speed> <len>\n\t" \
            "./slave 2 5000000 64\n");
#else
        printf("Usage: ./master <dev> <speed> <len> <udelay>\n\t" \
            "./master 1 5000000 64 100\n");
#endif
        exit(0);
    }

    good=bad=0;
    frame=1;
    sprintf(spi_dev,"/dev/spidev%s.0",argv[1]);
    speed=atoi(argv[2]);
    len= atoi(argv[3]);
    if(argc==5)
        udelay=atoi(argv[4]);

#ifdef SLAVE /* Run as Slave */
    rd=(char*)malloc(3);
    wr=(char*)malloc(len);
    printf("Slave: %s speed=%s len=%s\n", spi_dev, argv[2], argv[3]);

    file=spi_init(spi_dev);
    while(1){
        status=spi_slave_read(3,rd,file);

        if(*rd==0x01){
            frame=(*(rd+1)+(*(rd+2)<<8));
            *(wr)=frame&0xff;
            *(wr+1)=(frame>>8)&0xff;
            *(wr+2)=3;
            *(wr+3)=4;
            for(y=4; y<len-2; y++)
                *(wr+y)=(frame%0xff);
            crc=crc16(wr,len-2);
            *(wr+(len-1))=crc&0xff;
            *(wr+(len-2))=(crc>>8)&0xff;
#ifdef DEBUG
            dump(wr,10);
            printf("\n");
#endif
            spi_write(len,wr,file);
            if(last_frame==frame)
                printf("Slave resend frame %ld %d bytes \n", frame, len);
        }
        last_frame=frame;
    }
#else /* Run as Master */
    wr=(char*)malloc(3);
    rd=(char*)malloc(len);
    printf("Master: %s speed=%s len=%s\n", spi_dev, argv[2], argv[3]);
    x=0;
    file=spi_init(spi_dev);
    gettimeofday(&start, NULL);
    while(1){
        //gettimeofday(&start, NULL);
        spi_read(frame&0xff, ((frame>>8)&0xff), len, rd, file);
        if(*(rd+2)==3 && *(rd+3)==4){
            crc=crc16(rd,len);
            if(crc==0){
                good++;
                frame++;
            }else{
                bad++;
                printf("invalid crc %X for frame %ld. retry!\n", crc, frame);
            }
#ifdef DEBUG
            printf("Master Read ");
            dump(rd,10);
#endif
            gettimeofday(&finish, NULL);
            msec = timevaldiff(&start, &finish);
            if(frame%1000==0) /* show update every 1K frames */
                printf("Master read %ld kb in %ldms %ld/%ld\n", 
                    (len*frame)/1000, msec, good, bad);
        }else{
            bad++;
#ifdef DEBUG
            printf("Invalid data!\n");
            dump(rd,10);
#endif
        }

    }
#endif
    
    return 0;
}

