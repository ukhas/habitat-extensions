#!/usr/bin/make -f
# -*- makefile -*-

CFLAGS = -O2 -Wall -Werror -pedantic
LIBS = -ljsoncpp -lcurl -lpthread
headers = CouchDB.h EZ.h Uploader.h
cxxfiles = CouchDB.o EZ.o Uploader.o main.o
binary = cpp_connector

CXXFLAGS = $(CFLAGS)
objects = $(patsubst %.cxx,%.o,$(cxxfiles))

%.o : %.cxx $(headers)
	g++ -c $(CXXFLAGS) -o $@ $<

$(binary) : $(objects)
	g++ -o $@ $(objects) $(LIBS)

clean :
	rm -f $(objects) $(binary)

.PHONY : clean
.DEFAULT_GOAL := $(binary)
