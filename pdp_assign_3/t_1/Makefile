# Makefile to compile and run client and server, and plot results.

all: Client Server

Client: Client.cpp
	g++ -o Client Client.cpp -fopenmp

Server: Server.cpp
	g++ -o Server Server.cpp

runS: Server
	./Server $(ARG)

runC: Client
	./Client $(ARG)
	python3 plot.py

clean:
	rm -rf Client Server timings.csv
