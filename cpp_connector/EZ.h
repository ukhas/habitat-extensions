/* Copyright 2011 (C) Daniel Richman. License: GNU GPL 3; see LICENSE. */

#ifndef HABITATCPP_EZ_H
#define HABITATCPP_EZ_H

#include <string>
#include <iostream>
#include <stdexcept>
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
    Mutex &m;

public:
    MutexLock(Mutex &_m);
    ~MutexLock();
};

class cURL
{
    Mutex mutex;
    CURL *curl;

    /* You need to hold the mutex to use these four functions */
    void reset();
    string *perform(const string &url);
    template<typename T> void setopt(CURLoption option, T paramater);
    void setopt(CURLoption option, void *paramater);
    void setopt(CURLoption option, long parameter);

public:
    cURL();
    ~cURL();
    static string *escape(const string &s);
    string *get(const string &url);
    string *post(const string &url, const string &data);
    string *put(const string &url, const string &data);
};

class cURLGlobal
{
public:
    cURLGlobal() { curl_global_init(CURL_GLOBAL_ALL); };
    ~cURLGlobal() { curl_global_cleanup(); };
};

class cURLslist
{
    struct curl_slist *slist;

public:
    cURLslist() { slist = NULL; };
    void append(const char *s) { curl_slist_append(slist, s); };
    ~cURLslist() { curl_slist_free_all(slist); };
    const struct curl_slist *get() { return slist; };
};

class cURLError : public runtime_error
{
    cURLError(const string &what)
        : runtime_error("EZ::cURLError: " + what),
          error(CURLE_OK), function("") {};

    cURLError(CURLcode error, const string &function)
        : runtime_error("EZ::cURLError: " + function + ": " +
                        curl_easy_strerror(error)),
          error(error), function(function) {};

    friend class cURL;

public:
    const CURLcode error;
    const string function;
    ~cURLError() throw() {};
};

class HTTPResponse : public runtime_error
{
    HTTPResponse(long r)
        : runtime_error("EZ::HTTPResponse: " + r), response_code(r) {};

    friend class cURL;

public:
    const long response_code;
    ~HTTPResponse() throw() {};
};

} /* namespace EZ */

#endif /* HABITATCPP_EZ_H */
