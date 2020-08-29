TARGET = dvorak
LIBS = -lm -lX11
CC = gcc
CFLAGS = -g -Wall

.PHONY: default all clean

default: $(TARGET)
all: default

OBJECTS = $(patsubst %.c, %.o, $(wildcard *.c))

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -Wall $(LIBS) -o $@

clean:
	-rm -f *.o
	-rm -f $(TARGET)

install:
	cp $(TARGET) /usr/bin/
	cp 80-dvorak.rules /etc/udev/rules.d/
	cp dvorak@.service /etc/systemd/system/
	udevadm control --reload
	systemctl restart systemd-udevd.service
	systemctl daemon-reload
