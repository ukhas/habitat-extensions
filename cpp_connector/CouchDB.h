#ifndef HABITATCPP_COUCHDB_H
#define HABITATCPP_COUCHDB_H

#include <json/json.h>
#include <string>
#include <iostream>
#include <vector>
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
    vector<string> uuid_cache;
    EZ::Mutex uuid_cache_mutex;
    EZ::cURL curl;

    string next_uuid();
    friend class Database;

public:
    Server(const string &url);
    ~Server() {};
    Database operator[](const string &n) { return Database(*this, n); }
};

class Conflict
{
    const string doc_id;

public:
    Conflict(const string &d) : doc_id(d) {};
    ~Conflict() {};
    const string &get_info() { return doc_id; };
};

ostream &operator<<(ostream &o, Conflict &r);

} /* namespace CouchDB */

#endif /* HABITATCPP_COUCHDB_H */
