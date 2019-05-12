.SUFFIXES : .o.c

CC = gcc
# librerie e include per modbus
INCDIR = /home/reario/include
LIBDIR = /home/reario/lib

# librerie e include per postgres
LIBDB = /usr/local/pgsql/lib
DBINCDIR = /usr/include/postgresql/

#objs = cancello-read-from-pc.o cancello.o bit.o

all: cancello cancello-read-from-pc

cancello: bit.o cancello.o
	$(CC) -Wall -L${LIBDIR} -lmodbus $^ -o $@

cancello-read-from-pc: bit.o cancello-read-from-pc.o
	$(CC) -Wall -L${LIBDIR} -lmodbus $^ -o $@


# vengono costruiti file object
.c.o: gh.h
	$(CC) -c -g -DOTB -DDOINSERT3 -Wall -I${DBINCDIR} -I$(INCDIR)/modbus $< -o $@

# cancella i file non necessari e pulisce la directory, pronta per una compilazione pulita
clean :
	rm -f *~ *.o *.i *.s *.core cancello cancello-read-from-pc
