all: mcreversi

mcreversi: mcreversi.cpp
	g++ -std=c++11 -Wall -O3 --inline mcreversi.cpp -o mcreversi
