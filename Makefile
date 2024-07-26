#pkg-config from: https://www.geany.org/manual/gtk/glib/glib-compiling.html
#https://github.com/joprietoe/gdbus/blob/master/Makefile
#https://stackoverflow.com/questions/51269129/minimal-gdbus-client
TARGET = dvorak
CC = gcc
CFLAGS = -Wall -O3

.PHONY: default all clean install uninstall

default: all

all: dvorak.c
	$(CC) $(CFLAGS) -o $(TARGET) dvorak.c

clean:
	-rm -f *.o
	-rm -f $(TARGET)

install:
	cp dvorak /usr/local/bin/
	cp 80-dvorak.rules /etc/udev/rules.d/
	cp dvorak@.service /etc/systemd/system/
	udevadm control --reload
	systemctl restart systemd-udevd.service
	systemctl daemon-reload

uninstall:
	systemctl stop 'dvorak@*.service'
	rm /usr/local/bin/dvorak
	rm /etc/udev/rules.d/80-dvorak.rules
	rm /etc/systemd/system/dvorak@.service
	udevadm control --reload
	systemctl restart systemd-udevd.service
	systemctl daemon-reload
