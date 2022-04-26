#include <stdio.h>
#include <pigpio.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>


#define POWER_CTL     0x2D
#define ODR           0x28
#define STATUS        0x04
#define FIFO          0x11


const char codeVersion[3] = "1.0";  // code version number

const int timeDefault = 1;  // default duration of data stream, seconds
const int freqDefault = 1000;  // default frequency, Hz;
const int freqMax = 2000;  // maximal allowed cmdline arg sampling rate of data stream, Hz

const int speedSPI = 2000000;  // SPI communication speed, bps
const int freqMaxSPI = 100000;  // maximal possible communication sampling rate through SPI, Hz (assumption)

const double calib = (2.0*10.0)/(1<<20); // 20bits for +-10g 


void printUsage() {
    printf( "adxl345spi (version %s) \n"
            "License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>\n"
            "\n"
            "Usage: adxl345spi [OPTION]... \n"
            "Read data from ADXL345 accelerometer through SPI interface on Raspberry Pi.\n"
            "Online help, docs & bug reports: <https://github.com/nagimov/adxl345spi/>\n"
            "\n"
            "Mandatory arguments to long options are mandatory for short options too.\n"
            "  -s, --save FILE     save data to specified FILE (data printed to command-line\n"
            "                      output, if not specified)\n"
            "  -t, --time TIME     set the duration of data stream to TIME seconds\n"
            "                      (default: %i seconds) [integer]\n"
            "  -f, --freq FREQ     set the sampling rate of data stream to FREQ samples per\n"
            "                      second, 1 <= FREQ <= %i (default: %i Hz) [integer]\n"
            "\n"
            "Data is streamed in comma separated format, e. g.:\n"
            "  time,     x,     y,     z\n"
            "   0.0,  10.0,   0.0, -10.0\n"
            "   1.0,   5.0,  -5.0,  10.0\n"
            "   ...,   ...,   ...,   ...\n"
            "  time shows seconds elapsed since the first reading;\n"
            "  x, y and z show acceleration along x, y and z axis in fractions of <g>.\n"
            "\n"
            "Exit status:\n"
            "  0  if OK\n"
            "  1  if error occurred during data reading or wrong cmdline arguments.\n"
            "", codeVersion, timeDefault, freqMax, freqDefault);
}

int readBytes(int handle, char addr, char *data, int count) {
    char buff[2];
    buff[0] = (addr<<1)|1;
    buff[1] = 0x00;
    return spiXfer(handle, buff, data, count);
}

int writeBytes(int handle, char *data, int count) {
    data[0] = data[0]<<1;
    return spiWrite(handle, data, count);
}

//Downsample by getting closest data set
int alignData(double* rt, double *rx, double* ry, double* rz, double* at, double* ax, double* ay, double* az, int samples, int count, float delay){
    double tCurrent, tClosest, tError;
    
    //Start from this 0 offset
    at[0] = 0;
    ax[0] = rx[0];
    ay[0] = ry[0];
    az[0] = rz[0];
    
    int jClosest = 0;
    tClosest = rt[0];
    
    for (int i = 1; i < samples; i++) {
        tCurrent = (float)i * delay;
        tError = fabs(tClosest - tCurrent);
        for (int j = jClosest; j < count; j++) {  // lookup starting from previous j value
            if (fabs(rt[j] - tCurrent) <= tError) {  // in order to save some iterations
                jClosest = j;
                tClosest = rt[jClosest];
                tError = fabs(tClosest - tCurrent);
            }
            else {
                break;  // break the loop since the minimal error point passed
            }
        }  // when this loop is ended, jClosest and tClosest keep coordinates of the closest (j, t) point
        ax[i] = rx[jClosest];
        ay[i] = ry[jClosest];
        az[i] = rz[jClosest];
        at[i] = tCurrent;
    }
    return 0;
}

// Read FIFO, returns 1 if succesful
int readFIFO(int handle, int* x, int* y, int* z, char* data){
        readBytes(handle, FIFO, data, 10);
        
        if ((data[3]&0b10) || (data[6]&0b10) || (data[9]&0b10)) {return 0;} //Empty FIFO
        //~ if (!(data[3]&0b01)) {return 0;} // dataset validation
        
        *x = (int32_t)((((uint32_t)data[1])<<12)|(((uint32_t)data[2])<<4)|(((uint32_t)data[3])>>4));
        if (data[1]&0x80) *x = *x - (1<<20);
        *y = (int32_t)((((uint32_t)data[4])<<12)|(((uint32_t)data[5])<<4)|(((uint32_t)data[6])>>4));
        if (data[4]&0x80) *y = *y - (1<<20);
        *z = (int32_t)((((uint32_t)data[7])<<12)|(((uint32_t)data[8])<<4)|(((uint32_t)data[9])>>4));
        if (data[7]&0x80) *z = *z - (1<<20);
        return 1;
}


int main(int argc, char *argv[]) {   
    int bSave = 0;
    char vSave[256] = "";
    double vTime = timeDefault;
    double vFreq = freqDefault;
    
    for (int i = 1; i < argc; i++) { 
        if ((strcmp(argv[i], "-s") == 0) || (strcmp(argv[i], "--save") == 0)) {
            bSave = 1;
            if (i + 1 <= argc - 1) {  // make sure there are enough arguments in argv
                i++;
                strcpy(vSave, argv[i]);
            }
            else {
                printUsage();
                return 1;
            }
        }
        else if ((strcmp(argv[i], "-t") == 0) || (strcmp(argv[i], "--time") == 0)) {
            if (i + 1 <= argc - 1) {
                i++;
                vTime = atoi(argv[i]);
            }
            else {
                printUsage();
                return 1;
            }
        }
        else {
            printUsage();
            return 1;
        }
    }
    
    //Buffers to read and write stuffs to SPI
    char buff[2] = {0,0};
    char data[10] = {0,0,0,0,0,0,0,0,0,0};
    
    int32_t x, y, z;    
    
    // Initialize the board and connection
    if (gpioInitialise() < 0) {
        printf("Failed to initialize GPIO!");
        return 1;
    }
    
    // Initialize SPI Channel
    int h0 = spiOpen(0, speedSPI, 0);
    int h1 = spiOpen(1, speedSPI, 0);
    
    if(h0 < 0){
        printf("H0 Open error");
        return 1;
    }
    if (h1 < 0){
        printf("H1 Open error");
        return 1;
    }
    
    // Initialize the accelerelometer
    buff[0] = POWER_CTL;
    buff[1] = 0x00;
    writeBytes(h0, buff, 2);
    buff[0] = POWER_CTL;
    buff[1] = 0x00;
    writeBytes(h1, buff, 2);
    
    // Change the Output Data Rate
    buff[0] = ODR;
    buff[1] = 0x1; //2kHz
    writeBytes(h0, buff, 2);
    
    buff[0] = ODR;
    buff[1] = 0x1; //2kHz
    writeBytes(h1, buff, 2);
    
    // Misc
    int status0, status1;
        
    // Fake Cold Read
    for(int i = 0; i < 10; i++){
        readFIFO(h0, &x, &y, &z, data);
        readFIFO(h1, &x, &y, &z, data);
    }
    
    // Counters
    int count0, count1;
    count0 = 0;
    count1 = 0;
    double tStart = time_time();
    double t = time_time();
    
    // Array to store raw values
    int samplesMax = 1.1*freqMax*vTime;
    double *r0t, *r0x, *r0y, *r0z, *r1t, *r1x, *r1y, *r1z;
    r0t = malloc(samplesMax * sizeof(double));
    r0x = malloc(samplesMax * sizeof(double));
    r0y = malloc(samplesMax * sizeof(double));
    r0z = malloc(samplesMax * sizeof(double));
    r1t = malloc(samplesMax * sizeof(double));
    r1x = malloc(samplesMax * sizeof(double));
    r1y = malloc(samplesMax * sizeof(double));
    r1z = malloc(samplesMax * sizeof(double));
    
    //Real Reading
    while(t - tStart < vTime){     
        if(readFIFO(h0, &x, &y, &z, data)){
            t = time_time();
            printf("count0 = %d, t = %f, x = %f  y = %f z= %f\n", count0, t - tStart,x*calib,y*calib,z*calib);
            r0t[count0] = t-tStart;
            r0x[count0] = x*calib;
            r0y[count0] = y*calib;
            r0z[count0] = z*calib;
            count0++;
        }else

        if(readFIFO(h1, &x, &y, &z, data)){
            t = time_time();
            printf("count1 = %d, t = %f, x = %f  y = %f z= %f\n", count1, t - tStart,x*calib,y*calib,z*calib);
            r1t[count1] = t-tStart;
            r1x[count1] = x*calib;
            r1y[count1] = y*calib;
            r1z[count1] = z*calib;  
            count1++;
        }
        
        //~ // Test if overflow 
        //~ readBytes(h0, STATUS, buff, 2);
        //~ status0 = buff[1];
    
        //~ readBytes(h1, STATUS, buff, 2);
        //~ status1 = buff[1];
        
        //~ if(status0 == 7){
            //~ printf("Overflow0\n");
            //~ break;
        //~ }
        //~ if(status1 == 7){
            //~ printf("Overflow1\n");
            //~ break;
        //~ }

        
    }
    printf("t = %f\n", time_time() - tStart);
    
    
    if(!(bSave)){
        gpioTerminate();
        printf("Done\n");
        return 0;
    }
    
    //Write in File
    FILE *f;
    f = fopen(vSave, "w");
    fprintf(f, "time, x, y, z \n");
    for (int i = 0; i < count0; i++) {
        fprintf(f, "%.6f, %.6f, %.6f, %.6f\n", r0t[i], r0x[i], r0y[i], r0z[i]);
    }
    for(int i = 0; i < count1; i++){
        fprintf(f, "%.6f, %.6f, %.6f, %.6f\n", r1t[i], r1x[i], r1y[i], r1z[i]);
    }
    fclose(f);
    
    
    gpioTerminate();
    printf("Done\n");
    return 0;
}
