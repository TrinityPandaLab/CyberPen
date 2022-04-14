import os
import subprocess
from time import localtime, strftime
import keyboard


flag = 1
subject = 0
ctr = 4
folder = "CyberPenData/"
max_time = 3

subprocess.run(["sudo","killall","pigpiod"]) 

while(flag):
    print("Run: " + str(ctr))
    subprocess.run(["sudo","./adxl357spi","-t",str(max_time),"-s",folder + "sig" + str(subject).zfill(2)+ "_3_" + str(ctr).zfill(3) +".csv"]) 
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
