#version：0.1
#author: dongyue.z
#date:2016.01.29

CC = gcc 

CFLAGS= -w -O2 -fPIC

 #ver= DEBUG
 ##ifeq ($(ver), DEBUG)
 # CFLAGS += -D CO_DEBUG=1
 #endif


ifdef DEBUG
CFLAGS += -D CO_DEBUG=1
endif

TARGET = mempool_test
LIBSO_NAME = libco_mempool.so
LIBA_NAME = libco_mempool.a
SRC_PATH = .
SRCS = $(wildcard $(SRC_PATH)/*.c)
SRC_TMP = $(notdir $(SRCS))
#OBJS = $(SRC_TMP:%.c = %.o)
OBJS=co_mempool.o 
ARGV=-g
LIBSO = 
AR=ar -rs

all:lib

lib:${LIBSO_NAME} ${LIBA_NAME}

${LIBSO_NAME}:$(OBJS)
	$(CC) -shared  $(OBJS) $(LIBSO) -o $@

${LIBA_NAME}:$(OBJS)
	${AR} $@ $?

test: test.c 
	$(CC) $(CFLAGS) $(ARGV) $? $(LIBSO_NAME) -o $(TARGET)
	
%.o:%.c
	$(CC) $(CFLAGS) -c -o $@ $<

.PHONY: clean
clean:
	-rm ${TARGET}
	-rm ${LIBSO_NAME}
	-rm ${LIBA_NAME}
	-rm *.o -f
