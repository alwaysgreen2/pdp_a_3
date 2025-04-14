
CXX       := g++
CXXFLAGS  := -O3

all: Client Server

Client: Client.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

Server: Server.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

runS: Server
	./Server $(ARG)

runC: Client plot.py
	./Client $(ARG)
	python3 plot.py

clean:
	rm -f Client Server timings.csv

.PHONY: all runS runC clean
