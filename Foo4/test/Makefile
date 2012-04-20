SRCS = test-add.c
REV_OBJS = ${SRCS:.c=-output.o}
CATCH_OBJS = ${SRCS:.c=-catch.o}

test-objs = ${REV_OBJS} ${CATCH_OBJS}

all: tester

tester: ${test-objs} main.o
	g++ main.o ${test-objs}

main.o: main.cpp
	g++ -c main.cpp

.cpp.o:
	g++ -c $(CFLAGS) $(CPPFLAGS) -o $@ $<

test-add-output.o:
	../run.sh test-add test_add undo_test_add

clean:
	rm -f *.o *.bc *.ll *~
