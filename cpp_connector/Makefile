#!/usr/bin/make -f
# -*- makefile -*-

CFLAGS = -O2 -Wall -Werror -pedantic
LIBS = -ljsoncpp -lcurl -lpthread -lssl
headers = CouchDB.h EZ.h Uploader.h
cxxfiles = CouchDB.o EZ.o Uploader.o test_main.o
binary = cpp_connector
test_py_file = test_uploader.py

CXXFLAGS = $(CFLAGS)
objects = $(patsubst %.cxx,%.o,$(cxxfiles))

%.o : %.cxx $(headers)
	g++ -c $(CXXFLAGS) -o $@ $<

$(binary) : $(objects)
	g++ $(CXXFLAGS) -o $@ $(objects) $(LIBS)

test : $(binary) $(test_py_file)
	nosetests

clean :
	rm -f $(objects) $(binary) $(patsubst %.py,%.pyc,$(test_py_file))

.PHONY : clean test
.DEFAULT_GOAL := test
