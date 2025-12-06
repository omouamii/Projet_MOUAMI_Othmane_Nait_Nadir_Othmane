CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -fPIC
LDFLAGS = -shared
LIBS    = -lvirt

SRC = libvirt_helper.c
OBJ = $(SRC:.c=.o)
SO  = libvirt_helper.so

all: $(SO)

$(SO): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

%.o: %.c libvirt_helper.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(SO)
