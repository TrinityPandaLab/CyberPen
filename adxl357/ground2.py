import os, time, spidev
import RPi.GPIO as GPIO

# Setup Board
BUTTON_DRDY0 = 36
POWER_CTL = 0x2D
calib = 1.0/52428.8

#Setup Board
GPIO.setmode(GPIO.BOARD)

#Setup SPI
spi = spidev.SpiDev()
spi.open(0,0)
spi.max_speed_hz = 976000
spi.mode = 0b00


if __name__ == "__main__":
	spi.xfer([0x2D<<1, 0])
	response = spi.xfer2([(0x0<<1)|1,0])
	print(response)
	spi.close()
