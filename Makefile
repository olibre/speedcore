CXXFLAGS=-std=c++11
CXXFLAGS=-std=c++0x -pthread
CXXFLAGS += -Wall -Wextra
#CXXFLAGS += -g -O0
CXXFLAGS += -O3 -march=native
LDFLAGS += -pthread

all: speedcore

clean:
	rm -f speedcore
