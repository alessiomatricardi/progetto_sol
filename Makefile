# Makefile
# Alessio Matricardi
# LAST UPDATE 31/05/20

SRCDIR = ./src
LIBDIR = ./lib
BINDIR = .
INCDIR = ./include

CC 			= gcc
AR			= ar
CFLAGS 		= -Wall -g -pedantic -std=c99
ARFLAGS		= rvs
LIBS 		= -lstructure -lpthread
LDFLAGS 	= -L $(LIBDIR)
INCLUDES 	= -I $(INCDIR)
OPTFLAGS	= -O3 
EXE			= $(BINDIR)/supermercato


.PHONY: all test testquit clean cleanall

$(SRCDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -c -o $@ $<

$(EXE): $(SRCDIR)/supermercato.c $(LIBDIR)/libstructure.a $(SRCDIR)/signal_handler.o $(SRCDIR)/cliente.o $(SRCDIR)/cassa.o $(SRCDIR)/direttore.o $(SRCDIR)/parsing.o $(SRCDIR)/util.o $(SRCDIR)/filestat.o
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) $^ -o $@ $(LDFLAGS) $(LIBS)

$(LIBDIR)/libstructure.a: $(SRCDIR)/bqueue.o $(SRCDIR)/icl_hash.o $(SRCDIR)/queue.o
	$(AR) $(ARFLAGS) $@ $^
	
all: $(EXE)

test:
	./supermercato &
	\sleep 25
	\kill -s hup `cat ./var/run/sm.pid`
	\chmod +x analisi.sh
	\./analisi.sh
	
testquit:
	./supermercato &
	\sleep 25
	\kill -s quit `cat ./var/run/sm.pid`
	\chmod +x analisi.sh
	\./analisi.sh

clean:
	-rm -f $(EXE)
	clear
cleanall: clean
	-rm -f $(SRCDIR)/*.o $(LIBDIR)/*.a *~
	clear

