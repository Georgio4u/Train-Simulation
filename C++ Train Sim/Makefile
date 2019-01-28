all: train

train: train.o
	g++ -o train train.o

train.o: train.h train.cpp
	g++ -c -g -std=c++14 -ggdb -Wall train.cpp

clean:
	/bin/rm -f *.o train
