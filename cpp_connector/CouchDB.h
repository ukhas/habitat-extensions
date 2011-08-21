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

class Server
{
    const string url;
    vector<string> uuid_cache;
    EZ::Mutex uuid_cache_mutex;
    EZ::cURL curl;

public: /* XXX testing */
    string next_uuid();
    friend class Database;

public:
    Server(const string &url);
    ~Server() {};
};

class Database
{
    Server &server;
    string url;
    friend class Server;

public:
    Database(Server &server, string &db);
    ~Database() {};

    const Json::Value &operator[](const char *doc_id) const;
    Json::Value &operator[](const char *doc_id);
    const Json::Value &operator[](const string doc_id) const;
    Json::Value &operator[](const string doc_id);
};

} /* namespace CouchDB */

#endif /* HABITATCPP_COUCHDB_H */
