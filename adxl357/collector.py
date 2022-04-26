import os
import subprocess
from time import localtime, strftime
import keyboard


flag = 1
ctr = 0

subject_actual = 0
subject_taken = 3

folder = "CyberPenData/"
max_time = 3

subprocess.run(["sudo","killall","pigpiod"]) 

while(flag):
    print("Run: " + str(ctr))
    subprocess.run(["sudo","./adxl357spi","-t",str(max_time),"-s",folder + "sig_" + str(subject_actual).zfill(2)+ "_0_" + str(subject_taken).zfill(2) + "_" +   str(ctr).zfill(3) +".csv"]) 
    while(True):
        x = input("y to continue, q to quit, n to rewrite: ")
        if (x == "y"):
            ctr = ctr+1
            break
        elif (x == "q"):
            flag = 0
            break
        elif (x == "n"):
            break
        

'''
while (flag):
    # now = strftime("%m%d%H%M", localtime())
    subprocess.run(["sudo","./adxl345spi","-t",str(time),"-f","3200","-s",folder + "sig" + str(subject)+ "_" + "{3d}".format(ctr) + ".csv"])     
    # Motor
    time.sleep(2)
    
    ctr = ctr + 1
    #flag = 1 - flag

'''
