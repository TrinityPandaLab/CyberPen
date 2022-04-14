CC = gcc
CFLAGS = -Wall -pthread -lpigpio -lrt
INSTALL = `which install`

all: packages adxl357spi

install: adxl357spi
	$(INSTALL) ./adxl357spi /usr/local/bin/adxl357spi

clean:
	rm -f adxl357spi

packages:
	if ! dpkg-query -W -f='$${Status}' pigpio | grep "ok installed"; then apt-get -y install pigpio; fi

adxl357spi: adxl357spi.c
	$(CC) $(CFLAGS) adxl357spi.c -o adxl357spi
