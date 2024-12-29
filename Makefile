TARGET = http-proxy
SRCS = main.c src.c thread_pool.c
HEADER = header.h thread_pool.h
BUILD_DIR = build

CC = gcc
RM = rm
CFLAGS = -g -Wall
LIBS = -lpthread
INCLUDE_DIR = "."

all: ${BUILD_DIR} ${BUILD_DIR}/${TARGET}

${BUILD_DIR}:
	mkdir -p ${BUILD_DIR}

${BUILD_DIR}/${TARGET}: ${HEADER} ${SRCS}
	${CC} ${CFLAGS} -I${INCLUDE_DIR} ${SRCS} ${LIBS} -o ${BUILD_DIR}/${TARGET}

clean:
	${RM} -rf ${BUILD_DIR}
