#include "CouchDB.h"
#include <curl/curl.h>
#include <string>
#include "EZ.h"

using namespace std;

namespace CouchDB {

static const char *url_sep_after(const string &s)
{
    if (s.length() && s[s.length() - 1] != '/')
        return "/";
    else
        return "";
}

Server::Server(const string &url) : url(url + url_sep_after(url)) {}
Database::Database(Server &server, string &db) 
    : server(server), url(server.url + db + url_sep_after(db)) {}

string Server::next_uuid()
{
    EZ::MutexLock lock(&uuid_cache_mutex);
    string uuid;

    if (uuid_cache.size())
    {
        uuid = uuid_cache.back();
        uuid_cache.pop_back();
        return uuid;
    }
    else
    {
        string uuid_url = url;
        uuid_url.append("_uuids?count=100");

        string *response = curl.ez(uuid_url);

        Json::Reader reader;
        Json::Value root;
        
        if (!reader.parse(*response, root, false))
            throw "JSON Parsing error";

        delete response;

        const Json::Value uuids = root["uuids"];
        if (!uuids.isArray() || !uuids.size())
            throw "Invalid UUIDs response";

        uuid = uuids[Json::UInt(0)].asString();

        for (Json::UInt index = 1; index < uuids.size(); index++)
            uuid_cache.push_back(uuids[index].asString());
    }

    return uuid;
}

} /* namespace CouchDB */
