#include "EZ.h"
#include <curl/curl.h>
#include <string>
#include <memory>

using namespace std;

namespace EZ {

Mutex::Mutex()
{
    int result = pthread_mutex_init(&mutex, NULL);

    if (result != 0)
    {
        throw "Failed to create mutex";
    }
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

cURL::cURL()
{
    curl = curl_easy_init();

    if (curl == NULL)
    {
        throw "Failed to create curl";
    }
}

cURL::~cURL()
{
    curl_easy_cleanup(curl);
}

cURLError::cURLError(const string &info) 
    : error(CURLE_OK), extra(""), info(info) {}

cURLError::cURLError(CURLcode error, const string &extra) 
    : error(error), extra(extra)
{
    info = extra;
    info.append(": ");
    info.append(curl_easy_strerror(error));
}

ostream &operator<<(ostream &o, cURLError &e)
{
    o << e.get_info();
    return o;
}

ostream &operator<<(ostream &o, HTTPResponse &r)
{
    o << "HTTP " << r.get_response_code();
    return o;
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
        throw "curl_easy_escape failed";

    result_string = new string(result);
    curl_free(result);

    return result_string;
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
        source->written += max;
    }

    return write;
}

string *cURL::put(const string &url, const string &data)
{
    MutexLock lock(mutex);

    reset();
    setopt(CURLOPT_UPLOAD, 1);

    /* Disable Expect: 100-continue */
    cURLslist slist;
    slist.append("Expect:");
    setopt(CURLOPT_HTTPHEADER, slist.get());

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
