/* Copyright 2011 (C) Daniel Richman. License: GNU GPL 3; see COPYING. */

#ifndef HABITAT_EXTRACTOR_H
#define HABITAT_EXTRACTOR_H

#include <vector>
#include <jsoncpp/json.h>
#include "UploaderThread.h"
#include "EZ.h"

using namespace std;

namespace habitat {

enum push_flags
{
    PUSH_NONE = 0x00,
    PUSH_BAUDOT_HACK = 0x01
};

class Extractor;

class ExtractorManager
{
    EZ::Mutex mutex;
    vector<Extractor *> extractors;
    const Json::Value *current_payload;

public:
    UploaderThread &uthr;

    ExtractorManager(UploaderThread &u) : current_payload(NULL), uthr(u) {};
    virtual ~ExtractorManager() {};

    void add(Extractor &e);
    void skipped(int n);
    void push(char b, enum push_flags flags=PUSH_NONE);
    void payload(const Json::Value *set);
    const Json::Value *payload();

    virtual void status(const string &msg) = 0;
    virtual void data(const Json::Value &d) = 0;
};

class Extractor
{
protected:
    ExtractorManager *mgr;
    friend void ExtractorManager::add(Extractor &e);

public:
    virtual void skipped(int n) = 0;
    virtual void push(char b, enum push_flags flags) = 0;
};

} /* namespace habitat */

#endif /* HABITAT_EXTRACTOR_H */
