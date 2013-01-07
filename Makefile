# This is a sample Makefile which compiles source files named:
# - client.c
# - server.c
# and creating executables: "server", "client"
# It uses various standard libraries, and the copy of Stevens'
# library "libunp.a" in ~cse533/Stevens/unpv13e_solaris2.10 .
# It is set up, for illustrative purposes, to enable you to use
# the Stevens code in the ~cse533/Stevens/unpv13e_solaris2.10/lib
# subdirectory (where, for example, the file "unp.h" is located)
# without your needing to maintain your own, local copies of that
# code, and without your needing to include such code in the
# assignments

CC = gcc

LIBS = -lm -lresolv -lsocket -lnsl -lpthread\
	/home/courses/cse533/Stevens/unpv13e_solaris2.10/libunp.a\
	
FLAGS = -g -O2

CFLAGS = ${FLAGS} -I/home/courses/cse533/Stevens/unpv13e_solaris2.10/lib -I/home/courses/cse533/Asgn2_code 

all: client server  

# server uses the thread-safe version of readline.c

rtt.o: rtt.c
	${CC} ${CFLAGS} -c rtt.c
server: server.o get_ifi_info_plus.o rtt.o
	${CC} ${FLAGS} -o server server.o get_ifi_info_plus.o rtt.o ${LIBS}
server.o: server.c
	${CC} ${CFLAGS} -c server.c


client: client.o get_ifi_info_plus.o slidingwindow.o rtt.o
	${CC} ${FLAGS} -o client client.o get_ifi_info_plus.o slidingwindow.o rtt.o ${LIBS}
client.o: client.c
	${CC} ${CFLAGS} -c client.c
slidingwindow.o: slidingwindow.c
	${CC} ${CFLAGS} -c slidingwindow.c

# produce the binary of get_ifi_info_plus.c so that it can be linked in client and server

get_ifi_info_plus.o: get_ifi_info_plus.c
	${CC} ${CFLAGS} -c get_ifi_info_plus.c


clean:
	rm client client.o server server.o get_ifi_info_plus.o slidingwindow.o rtt.o

