SRCS = test-add.c test-add2.c test-sub.c test-sub2.c test-mul.c test-mul2.c  \
test-if.c test-if2.c test-if3.c test-if-else.c test-while.c test-while2.c    \
test-while3.c test-for.c test-array.c test-random.c test-ptr.c test-struct.c \
test-assign.c test-assign-in-struct.c test-assign-struct-args.c              \
test-augment-struct.c test-assign-struct-args-if.c test-switch.c
REV_OBJS = ${SRCS:.c=-output.o}
CATCH_OBJS = ${SRCS:.c=-catch.o}

test-objs = ${REV_OBJS} ${CATCH_OBJS}

all: tester
	./tester

tester: ${test-objs} main.o
	clang++ -o tester main.o ${test-objs}

.cpp.o:
	clang++ -c $(CXXFLAGS) -o $@ $<

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

test-if-else-output.o:
	../run.sh test-if-else test_if_else undo_test_if_else

test-while-output.o:
	../run.sh test-while test_while undo_test_while

test-while2-output.o:
	../run.sh test-while2 test_while2 undo_test_while2

test-while3-output.o:
	../run.sh test-while3 test_while3 undo_test_while3

test-for-output.o:
	../run.sh test-for test_for undo_test_for

test-array-output.o:
	../run.sh test-array test_array undo_test_array

test-random-output.o:
	../run.sh test-random test_random undo_test_random

test-ptr-output.o:
	../run.sh test-ptr test_ptr undo_test_ptr

test-struct-output.o:
	../run.sh test-struct test_struct undo_test_struct

test-assign-output.o:
	../run.sh test-assign test_assign undo_test_assign

test-assign-in-struct-output.o:
	../run.sh test-assign-in-struct test_assign_in_struct \
	 undo_test_assign_in_struct

test-assign-struct-args-output.o:
	../run.sh test-assign-struct-args test_assign_struct_args \
	undo_test_assign_struct_args

test-augment-struct-output.o:
	../run.sh test-augment-struct test_augment_struct \
	undo_test_augment_struct

test-assign-struct-args-if-output.o:
	../run.sh test-assign-struct-args-if test_assign_struct_args_if \
	undo_test_assign_struct_args_if

test-switch-output.o:
	../run.sh test-switch test_switch undo_test_switch

phold-output.o: preGenerated/phold.bc
	cp ./preGenerated/phold.bc .
	../run.sh phold phold_event_handler rc 1

airport-output.o: preGenerated/airport.bc
	cp ./preGenerated/airport.bc .
	../run.sh airport event_handler rc 1

pcs-output.o: preGenerated/pcs.bc
	cp ./preGenerated/pcs.bc .
	../run.sh pcs Cell_EventHandler rc 1

clean:
	rm -f *.o *.s *.bc *.ll *~
