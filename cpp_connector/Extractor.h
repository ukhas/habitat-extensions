/* Copyright 2011 (C) Daniel Richman. License: GNU GPL 3; see COPYING. */

#ifndef HABITAT_EXTRACTOR_H
#define HABITAT_EXTRACTOR_H

namespace habitat {

#include <vector>
#include <json/json.h>
#include "habitat/UploaderThread.h"

enum push_flags
{
    PUSH_NONE = 0x00;
    PUSH_BAUDOT_HACK = 0x01;
};

class Extractor;

class ExtractorManager
{
    vector<Extractor *> extractors;

public:
    void skipped(int n);
    void push(char b, enum push_flags=PUSH_NONE);
    void add(Extractor &e);

    void status(const string &msg);
    void data(Json::Value &d);
};

class Extractor
{
protected:
    ExtractorManager *mgr;
    friend class ExtractorManager;

    virtual void skipped(int n) = 0;
    virtual void push(char b, enum push_flags) = 0;
};

} /* namespace habitat */

#endif /* HABITAT_EXTRACTOR_H */
