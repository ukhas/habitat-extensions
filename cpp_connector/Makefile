#!/usr/bin/make -f
# -*- makefile -*-
# Copyright 2011 (C) Daniel Richman. License: GNU GPL 3; see LICENSE.

CFLAGS = -pthread -O2 -Wall -Werror -pedantic -Wno-variadic-macros -Isrc
upl_libs = -pthread -ljsoncpp -lcurl -lssl
ext_libs = -ljsoncpp

test_py_files = tests/test_uploader.py tests/test_extractor.py
headers = src/CouchDB.h src/EZ.h src/Uploader.h src/UploaderThread.h \
          src/Extractor.h src/UKHASExtractor.h \
		  tests/test_extractor_mock_UploaderThread.h
upl_cxxfiles = src/CouchDB.cxx src/EZ.cxx src/Uploader.cxx
upl_thr_cflags = -DTHREADED
upl_nrm_binary = tests/cpp_connector
upl_nrm_objects = tests/test_uploader_main.o
upl_thr_binary = tests/cpp_connector_threaded
upl_thr_objects = src/UploaderThread.o tests/test_uploader_main.threaded.o
ext_cxxfiles = src/Extractor.cxx src/UKHASExtractor.cxx \
               tests/test_extractor_main.cxx
ext_binary = tests/extractor
ext_mock_cflags = -include tests/test_extractor_mock_UploaderThread.h

CXXFLAGS = $(CFLAGS)
upl_objects = $(patsubst %.cxx,%.o,$(upl_cxxfiles))
ext_objects = $(patsubst %.cxx,%.ext_mock.o,$(ext_cxxfiles))

%.o : %.cxx $(headers)
	g++ -c $(CXXFLAGS) -o $@ $<

%.threaded.o : %.cxx $(headers)
	g++ -c $(CXXFLAGS) $(upl_thr_cflags) -o $@ $<

%.ext_mock.o : %.cxx $(headers)
	g++ -c $(CXXFLAGS) $(ext_mock_cflags) -o $@ $<

$(upl_nrm_binary) : $(upl_objects) $(upl_nrm_objects)
	g++ $(CXXFLAGS) -o $@ $(upl_objects) $(upl_nrm_objects) $(upl_libs)

$(upl_thr_binary) : $(upl_objects) $(upl_thr_objects)
	g++ $(CXXFLAGS) -o $@ $(upl_objects) $(upl_thr_objects) $(upl_libs)

$(ext_binary) : $(ext_objects)
	g++ $(CXXFLAGS) -o $@ $(ext_objects) $(ext_libs)

test : $(upl_nrm_binary) $(upl_thr_binary) $(ext_binary) $(test_py_files)
	nosetests

clean :
	rm -f $(upl_objects) $(upl_nrm_objects) $(upl_thr_objects) \
	      $(upl_nrm_binary) $(upl_thr_binary) \
		  $(ext_objects) $(ext_binary) \
	      $(patsubst %.py,%.pyc,$(test_py_files))

.PHONY : clean test
.DEFAULT_GOAL := test
