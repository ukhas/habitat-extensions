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
    static const map<string,string> view_default_options;
    Server &server;
    string url;
    friend class Server;

    string make_doc_url(const string &doc_id) const;

public:
    Database(Server &server, const string &db);
    ~Database() {};

    void save_doc(Json::Value &doc);
    Json::Value *get_doc(const string &doc_id) const;
    Json::Value *operator[](const string &doc_id);
    Json::Value *view(const string &design_doc, const string &view_name,
                      const map<string,string> &options=view_default_options)
                      const;
    static string json_query_value(Json::Value &value);
};

class Server
{
    const string url;
    deque<string> uuid_cache;
    EZ::Mutex uuid_cache_mutex;
    EZ::cURL curl;

    string next_uuid();
    friend class Database;

    Json::Value *get_json(const string &get_url);

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
