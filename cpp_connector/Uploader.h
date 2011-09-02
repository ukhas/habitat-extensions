/* Copyright 2011 (C) Daniel Richman. License: GNU GPL 3; see LICENSE. */

#ifndef HABITATCPP_UPLOADER_H
#define HABITATCPP_UPLOADER_H

#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>
#include <json/json.h>
#include "EZ.h"
#include "CouchDB.h"

using namespace std;

namespace habitat {

class UnmergeableError : public runtime_error
{
public:
    UnmergeableError() : runtime_error("habitat::UnmergeableError") {};
    UnmergeableError(const string &what) : runtime_error(what) {};
};

class CollisionError : public runtime_error
{
public:
    CollisionError() : runtime_error("habitat::CollisionError") {};
    CollisionError(const string &what) : runtime_error(what) {};
};

class Uploader
{
    EZ::Mutex mutex;
    string callsign;
    CouchDB::Server server;
    CouchDB::Database database;
    int max_merge_attempts;
    string latest_listener_info;
    string latest_listener_telemetry;

    string listener_doc(const char *type, const Json::Value &data,
                        int time_created);

public:
    Uploader(const string &callsign,
             const string &couch_uri="http://habhub.org",
             const string &couch_db="habitat",
             int max_merge_attempts=20);
    ~Uploader() {};
    string payload_telemetry(const string &data,
                             const Json::Value &metadata=Json::Value::null,
                             int time_created=-1);
    string listener_telemetry(const Json::Value &data, int time_created=-1);
    string listener_info(const Json::Value &data, int time_created=-1);
    vector<Json::Value> *flights();
};

} /* namespace habitat */

#endif
