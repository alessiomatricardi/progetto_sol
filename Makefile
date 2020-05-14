# Makefile
# Alessio Matricardi

SRCDIR = ./src
LIBDIR = ./lib
BINDIR = ./bin
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


.PHONY: all test clean cleanall

$(SRCDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -c -o $@ $<

#da completare
$(EXE): $(SRCDIR)/supermercato.c $(LIBDIR)/libstructure.a $(SRCDIR)/parsing.o
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) $^ -o $@ $(LDFLAGS) $(LIBS)

$(LIBDIR)/libstructure.a: $(SRCDIR)/bqueue.o $(SRCDIR)/icl_hash.o
	$(AR) $(ARFLAGS) $@ $^
	
all: $(EXE)

test:

clean:
	-rm -f $(EXE)
	clear
cleanall	: clean
	\rm -f $(SRCDIR)/*.o $(LIBDIR)/*.a *~
	clear

