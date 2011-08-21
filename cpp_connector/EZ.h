#ifndef HABITATCPP_EZ_H
#define HABITATCPP_EZ_H

#include <string>
#include <iostream>
#include <curl/curl.h>

using namespace std;

namespace EZ {

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

class cURLGlobal
{
public:
    cURLGlobal() { curl_global_init(CURL_GLOBAL_ALL); };
    ~cURLGlobal() { curl_global_cleanup(); };
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

} /* namespace EZ */

#endif /* HABITATCPP_EZ_H */
