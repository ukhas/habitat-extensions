/* Copyright 2011 (C) Daniel Richman. License: GNU GPL 3; see LICENSE. */

#include "EZ.h"
#include <curl/curl.h>
#include <pthread.h>
#include <string>
#include <memory>
#include <stdexcept>
#include <sstream>

using namespace std;

namespace EZ {

Mutex::Mutex()
{
    int result = pthread_mutex_init(&mutex, NULL);

    if (result != 0)
        throw runtime_error("Failed to create mutex");
}

Mutex::~Mutex()
{
    pthread_mutex_destroy(&mutex);
}

MutexLock::MutexLock(Mutex &_m) : m(_m)
{
    pthread_mutex_lock(&(m.mutex));
}

MutexLock::~MutexLock()
{
    pthread_mutex_unlock(&(m.mutex));
}

static string http_response_string(long r)
{
    stringstream ss;
    ss << "EZ::HTTPResponse: HTTP " << r;
    return ss.str();
}

HTTPResponse::HTTPResponse(long r)
    : runtime_error(http_response_string(r)), response_code(r) {}

cURL::cURL()
{
    curl = curl_easy_init();

    if (curl == NULL)
    {
        throw runtime_error("Failed to create curl");
    }
}

cURL::~cURL()
{
    curl_easy_cleanup(curl);
}

string *cURL::escape(const string &s)
{
    char *result;
    string *result_string;

    /* cURL wants a handle passed to easy escape for some reason.
     * As far as I can tell it doesn't use it... */
    cURL escaper;
    MutexLock lock(escaper.mutex);

    result = curl_easy_escape(escaper.curl, s.c_str(), s.length());

    if (result == NULL)
        throw runtime_error("curl_easy_escape failed");

    result_string = new string(result);
    curl_free(result);

    return result_string;
}

string cURL::query_string(const map<string,string> &options,
                          bool add_questionmark)
{
    string result;

    if (add_questionmark)
        result.append("?");

    map<string,string>::const_iterator it;

    for (it = options.begin(); it != options.end(); it++)
    {
        if (it != options.begin())
            result.append("&");

        string *key_escaped = escape((*it).first);
        auto_ptr<string> destroyer_1(key_escaped);

        string *value_escaped = escape((*it).second);
        auto_ptr<string> destroyer_2(value_escaped);

        result.append(*key_escaped);
        result.append("=");
        result.append(*value_escaped);
    }

    return result;
}

void cURL::reset()
{
    curl_easy_reset(curl);
}

template<typename T> void cURL::setopt(CURLoption option, T parameter)
{
    CURLcode result = curl_easy_setopt(curl, option, parameter);
    if (result != CURLE_OK)
        throw cURLError(result, "curl_easy_setopt");
}

string *cURL::get(const string &url)
{
    MutexLock lock(mutex);

    reset();
    return cURL::perform(url);
}

string *cURL::post(const string &url, const string &data)
{
    MutexLock lock(mutex);

    reset();
    setopt(CURLOPT_POSTFIELDS, data.c_str());
    setopt(CURLOPT_POSTFIELDSIZE, data.length());

    return cURL::perform(url);
}

struct read_func_userdata
{
    const string *data;
    size_t written;
};

static size_t read_func(void *ptr, size_t size, size_t nmemb, void *userdata)
{
    struct read_func_userdata *source =
        static_cast<struct read_func_userdata *>(userdata);
    char *target = static_cast<char *>(ptr);
    size_t max = size * nmemb;
    size_t remaining = source->data->length() - source->written;

    size_t write = remaining;
    if (write > max)
        write = max;

    if (write)
    {
        source->data->copy(target, write, source->written);
        source->written += write;
    }

    return write;
}

string *cURL::put(const string &url, const string &data)
{
    MutexLock lock(mutex);

    reset();
    setopt(CURLOPT_UPLOAD, 1);

    struct read_func_userdata userdata;
    userdata.data = &data;
    userdata.written = 0;

    setopt(CURLOPT_READFUNCTION, read_func);
    setopt(CURLOPT_READDATA, &userdata);
    setopt(CURLOPT_INFILESIZE, data.length());

    return cURL::perform(url);
}

static size_t write_func(char *data, size_t size, size_t nmemb, void *userdata)
{
    size_t length = size * nmemb;
    string *target = static_cast<string *>(userdata);

    target->append(data, length);
    return length;
}

string *cURL::perform(const string &url)
{
    auto_ptr<string> response(new string);

    setopt(CURLOPT_URL, url.c_str());
    setopt(CURLOPT_WRITEFUNCTION, write_func);
    setopt(CURLOPT_WRITEDATA, response.get());

    CURLcode result;
    result = curl_easy_perform(curl);
    if (result != CURLE_OK)
        throw cURLError(result, "curl_easy_perform");

    long response_code;
    result = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    if (result != CURLE_OK)
        throw cURLError(result, "curl_easy_getinfo");

    if (response_code < 200 || response_code > 299)
        throw HTTPResponse(response_code);

    return response.release();
}

} /* namespace EZ */
