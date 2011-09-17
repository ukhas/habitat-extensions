#!/usr/bin/make -f
# -*- makefile -*-
# Copyright 2011 (C) Daniel Richman. License: GNU GPL 3; see LICENSE.

CFLAGS = -pthread -O2 -Wall -Werror -pedantic -Wno-variadic-macros
LIBS = -pthread -ljsoncpp -lcurl -lssl

headers = CouchDB.h EZ.h Uploader.h UploaderThread.h
cxxfiles = CouchDB.cxx EZ.cxx Uploader.cxx
test_py_file = test_uploader.py
test_threaded_cflags = -DTHREADED

normal_binary = cpp_connector
normal_objects = test_main.o
threaded_binary = cpp_connector_threaded
threaded_objects = UploaderThread.o test_main.threaded.o

CXXFLAGS = $(CFLAGS)
objects = $(patsubst %.cxx,%.o,$(cxxfiles))

%.o : %.cxx $(headers)
	g++ -c $(CXXFLAGS) -o $@ $<

%.threaded.o : %.cxx $(headers)
	g++ -c $(CXXFLAGS) $(test_threaded_cflags) -o $@ $<

$(normal_binary) : $(objects) $(normal_objects)
	g++ $(CXXFLAGS) -o $@ $(objects) $(normal_objects) $(LIBS)

$(threaded_binary) : $(objects) $(threaded_objects)
	g++ $(CXXFLAGS) -o $@ $(objects) $(threaded_objects) $(LIBS)

test : $(normal_binary) $(threaded_binary) $(test_py_file)
	nosetests

clean :
	rm -f $(objects) $(normal_objects) $(threaded_objects) \
	    $(normal_binary) $(threaded_binary) \
	    $(patsubst %.py,%.pyc,$(test_py_file))

.PHONY : clean test
.DEFAULT_GOAL := test
