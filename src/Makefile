CC=gcc
CFLAGS=-I. -I../include -Wall -Wno-unused-result -O2
DEPS = gec_lock.h
OBJ =  comm-host.o comm-dev.o comm-lpc.o comm-i2c.o cb_hddledd.o misc_util.o csem.o ipc_lock.o gec_lock.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

cb-hddledd: $(OBJ)
	gcc -o $@ $^ $(CFLAGS)

clean:
	rm -f *.o *~ cb-hddledd
