.PHONY: all test clean

all: hw2.o queue.o
	gcc -o hw2 hw2.c queue.o -lpthread
hw2.o: queue.o
queue.o: queue.h
test:
	./test.txt
clean:
	$(RM) ./hw2 ./*.o