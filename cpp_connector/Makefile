#!/usr/bin/make -f
# -*- makefile -*-

CFLAGS = -O2 -Wall -Werror -pedantic
LIBS = -ljsoncpp -lcurl -lpthread -lssl

headers = CouchDB.h EZ.h Uploader.h UploaderThread.h
cxxfiles = CouchDB.cxx EZ.cxx Uploader.cxx

normal_cxxfiles = test_main.cxx
normal_binary = cpp_connector

thread_cxxfiles = UploaderThread.cxx test_main_threaded.cxx
thread_binary = cpp_connector_threaded

test_py_file = test_uploader.py

CXXFLAGS = $(CFLAGS)
normal_objects = $(patsubst %.cxx,%.o,$(cxxfiles) $(normal_cxxfiles))
thread_objects = $(patsubst %.cxx,%.o,$(cxxfiles) $(thread_cxxfiles))

%.o : %.cxx $(headers)
	g++ -c $(CXXFLAGS) -o $@ $<

$(normal_binary) : $(normal_objects)
	g++ $(CXXFLAGS) -o $@ $(normal_objects) $(LIBS)

$(thread_binary) : $(thread_objects)
	g++ $(CXXFLAGS) -o $@ $(thread_objects) $(LIBS)

test : $(normal_binary) $(thread_binary) $(test_py_file)
	nosetests

clean :
	rm -f $(objects) $(binary) $(patsubst %.py,%.pyc,$(test_py_file))

.PHONY : clean test
.DEFAULT_GOAL := test
