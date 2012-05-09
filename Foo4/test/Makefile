SRCS = test-add.c test-add2.c test-sub.c test-sub2.c test-mul.c test-mul2.c \
test-if.c test-if2.c test-if3.c
REV_OBJS = ${SRCS:.c=-output.o}
CATCH_OBJS = ${SRCS:.c=-catch.o}

test-objs = ${REV_OBJS} ${CATCH_OBJS}

all: tester
	./tester

tester: ${test-objs} main.o
	g++ -o tester main.o ${test-objs}

.cpp.o:
	g++ -c $(CFLAGS) $(CPPFLAGS) -o $@ $<

test-add-output.o:
	../run.sh test-add test_add undo_test_add

test-add2-output.o:
	../run.sh test-add2 test_add2 undo_test_add2

test-sub-output.o:
	../run.sh test-sub test_sub undo_test_sub

test-sub2-output.o:
	../run.sh test-sub2 test_sub2 undo_test_sub2

test-mul-output.o:
	../run.sh test-mul test_mul undo_test_mul

test-mul2-output.o:
	../run.sh test-mul2 test_mul2 undo_test_mul2

test-if-output.o:
	../run.sh test-if test_if undo_test_if

test-if2-output.o:
	../run.sh test-if2 test_if2 undo_test_if2

test-if3-output.o:
	../run.sh test-if3 test_if3 undo_test_if3

clean:
	rm -f *.o *.s *.bc *.ll *~
