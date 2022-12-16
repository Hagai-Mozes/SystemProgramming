all: hw2.o
	gcc hw2.c queue.o worker.o -o hw2 -lpthread
hw2.o: worker.o
worker.o: queue.o worker.h
queue.o: queue.h

clean:
	rm ./hw2 *.o count*.txt thread*.txt dispatcher.txt