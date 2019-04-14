.SUFFIXES : .o.c

CC = gcc
# librerie e include per modbus
INCDIR = /home/reario/include
LIBDIR = /home/reario/lib

# librerie e include per postgres
LIBDB = /usr/local/pgsql/lib
DBINCDIR = /usr/include/postgresql/

objs = cancello.o bit.o

all: cancello

cancello: bit.o cancello.o
	$(CC) -Wall -L${LIBDIR} -lmodbus $^ -o $@


# vengono costruiti fli object
.c.o: gh.h
	$(CC) -c -g -DOTB -DDOINSERT3 -Wall -I${DBINCDIR} -I$(INCDIR)/modbus $< -o $@

# cancella i file non necessari e pulisce la directory, pronta per una compilazione pulita
clean :
	rm -f *~ *.o *.i *.s *.core cancello
