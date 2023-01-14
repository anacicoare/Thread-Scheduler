build: libscheduler.so move

libscheduler.so: so_scheduler.o priority_queue.o
	gcc -Wall -shared so_scheduler.o priority_queue.o -o libscheduler.so

so_scheduler.o: so_scheduler.c
	gcc -Wall -fPIC -o so_scheduler.o -c -lpthread so_scheduler.c

priority_queue.o: priority_queue.c
	gcc -Wall -fPIC -o priority_queue.o -c priority_queue.c

move: libscheduler.so
	mv libscheduler.so ../checker-lin
	cd ../checker-lin && make -f Makefile.checker

.PHONY: clean

clean:
	rm -f *.o libscheduler.so