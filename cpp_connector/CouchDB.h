#ifndef HABITATCPP_COUCHDB_H
#define HABITATCPP_COUCHDB_H

#include <json/json.h>
#include <string>
#include <iostream>
#include <vector>
#include <curl/curl.h>

using namespace std;

namespace CouchDB {

class Mutex
{
    pthread_mutex_t mutex;
    friend class MutexLock;

public:
    Mutex();
    ~Mutex();
};

class MutexLock
{
    Mutex *m;

public:
    MutexLock(Mutex *_m);
    ~MutexLock();
};

class cURL
{
    Mutex mutex;
    CURL *curl;

public:
    cURL();
    ~cURL();
    static string *escape(const string &s);
    string *ez(const string &url, const string &data="", int post=0);
};

class cURLError
{
    const CURLcode error;
    const string extra;
    string info;

    cURLError(const string &extra);
    cURLError(CURLcode error, const string &extra);
    friend class cURL;

public:
    const string &get_info() { return info; };
    CURLcode get_code() { return error; };
    ~cURLError() {};
};

ostream &operator<<(ostream &o, cURLError &e);

class Server
{
    const string url;
    vector<string> uuid_cache;
    Mutex uuid_cache_mutex;
    cURL curl;

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
