# Simple Makefile to build the single-file program
CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2
SRC = yogeshwari_encrypter_kavi.cpp
OUT = yogeshwari_encrypter_kavi

all: build

build:
	$(CXX) $(CXXFLAGS) "$(SRC)" -o $(OUT)

clean:
	-@rm -f $(OUT) *.exe *.o *.tmp *.bmp *.wav *.png

.PHONY: all build clean
