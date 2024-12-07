TARGET_2 = http-proxy
SRCS_2 = main.c src.c

CC=gcc
RM=rm
CFLAGS= -g -Wall
LIBS=-lpthread
INCLUDE_DIR="."

all: ${TARGET_2}

${TARGET_2}: header.h ${SRCS_2}
	${CC} ${CFLAGS} -I${INCLUDE_DIR} ${SRCS_2} ${LIBS} -o ${TARGET_2}

clean:
	${RM} -f *.o ${TARGET_2}
