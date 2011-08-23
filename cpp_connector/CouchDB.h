/* Copyright 2011 (C) Daniel Richman. License: GNU GPL 3; see LICENSE. */

#ifndef HABITATCPP_COUCHDB_H
#define HABITATCPP_COUCHDB_H

#include <json/json.h>
#include <string>
#include <iostream>
#include <deque>
#include <stdexcept>
#include <curl/curl.h>

#include "EZ.h"

using namespace std;

namespace CouchDB {

class Server;

class Database
{
    Server &server;
    string url;
    friend class Server;

public:
    Database(Server &server, const string &db);
    ~Database() {};

    void save_doc(Json::Value &doc);
    Json::Value *get_doc(const string &doc_id);
    Json::Value *operator[](const string &doc_id);
};

class Server
{
    const string url;
    deque<string> uuid_cache;
    EZ::Mutex uuid_cache_mutex;
    EZ::cURL curl;

    string next_uuid();
    friend class Database;

public:
    Server(const string &url);
    ~Server() {};
    Database operator[](const string &n) { return Database(*this, n); }
};

class Conflict : public runtime_error
{
    Conflict(const string &doc_id)
        : runtime_error("CouchDB::Conflict: " + doc_id), doc_id(doc_id) {};

    friend class Database;

public:
    const string doc_id;
    ~Conflict() throw() {};
};

} /* namespace CouchDB */

#endif /* HABITATCPP_COUCHDB_H */
