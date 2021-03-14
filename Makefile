#pkg-config from: https://www.geany.org/manual/gtk/glib/glib-compiling.html
#https://github.com/joprietoe/gdbus/blob/master/Makefile
#https://stackoverflow.com/questions/51269129/minimal-gdbus-client
TARGET = dvorak
CC = gcc
CFLAGS = -g -Wall

.PHONY: default all clean

default: $(TARGET)
all: default

OBJECTS = $(patsubst %.c, %.o, $(wildcard *.c))

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@

clean:
	-rm -f *.o
	-rm -f $(TARGET)

install:
	cp dvorak /usr/local/bin/
	cp dvorak.sh /usr/local/sbin/
	cp 80-dvorak.rules /etc/udev/rules.d/
	cp dvorak@.service /etc/systemd/system/
	udevadm control --reload
	systemctl restart systemd-udevd.service
	systemctl daemon-reload

uninstall:
	rm /usr/local/bin/dvorak
	rm /usr/local/sbin/dvorak.sh
	rm /etc/udev/rules.d/80-dvorak.rules
	rm /etc/systemd/system/dvorak@.service
	udevadm control --reload
	systemctl restart systemd-udevd.service
	systemctl daemon-reload
