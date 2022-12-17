all: hw2.o
	gcc -o hw2 hw2.c queue.o
hw2.o: queue.o
queue.o: queue.h
clean:
	rm ./hw2 *.o count*.txt thread*.txt dispatcher.txt stats.txt