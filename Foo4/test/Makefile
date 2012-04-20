SRCS = test-add.c test-add2.c
REV_OBJS = ${SRCS:.c=-output.o}
CATCH_OBJS = ${SRCS:.c=-catch.o}

test-objs = ${REV_OBJS} ${CATCH_OBJS}

all: tester

tester: ${test-objs} main.o
	g++ -o tester main.o ${test-objs}

.cpp.o:
	g++ -c $(CFLAGS) $(CPPFLAGS) -o $@ $<

test-add-output.o:
	../run.sh test-add test_add undo_test_add

test-add2-output.o:
	../run.sh test-add2 test_add2 undo_test_add2

clean:
	rm -f *.o *.s *.bc *.ll *~
