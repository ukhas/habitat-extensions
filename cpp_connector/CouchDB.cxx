/* Copyright 2011 (C) Daniel Richman. License: GNU GPL 3; see LICENSE. */

#include "CouchDB.h"
#include <curl/curl.h>
#include <string>
#include <memory>
#include <stdexcept>
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

Server::Server(const string &url)
    : url(url + url_sep_after(url))
{
    if (!url.length())
        throw invalid_argument("URL of zero length");
}

Database::Database(Server &server, const string &db)
    : server(server), url(server.url + db + url_sep_after(db))
{
    if (!db.length())
        throw invalid_argument("DB of zero length");
}

string Server::next_uuid()
{
    EZ::MutexLock lock(uuid_cache_mutex);
    string uuid;

    if (uuid_cache.size())
    {
        uuid = uuid_cache.back();
        uuid_cache.pop_back();
        return uuid;
    }
    else
    {
        string uuid_url(url);
        uuid_url.append("_uuids?count=100");

        string *response = curl.get(uuid_url);
        auto_ptr<string> response_destroyer(response);

        Json::Reader reader;
        Json::Value root;

        if (!reader.parse(*response, root, false))
            throw runtime_error("JSON Parsing error");

        response_destroyer.reset();

        const Json::Value uuids = root["uuids"];
        if (!uuids.isArray() || !uuids.size())
            throw runtime_error("Invalid UUIDs response");

        uuid = uuids[Json::UInt(0)].asString();

        for (Json::UInt index = 1; index < uuids.size(); index++)
            uuid_cache.push_back(uuids[index].asString());
    }

    return uuid;
}

Json::Value *Database::operator[](const string &doc_id)
{
    return get_doc(doc_id);
}

void Database::save_doc(Json::Value &doc)
{
    Json::Value &id = doc["_id"];

    if (id.isNull())
        id = server.next_uuid();

    if (!id.isString())
        throw runtime_error("_id must be a string if set");

    string doc_id = id.asString();

    if (doc_id.length() == 0)
        throw runtime_error("_id cannot be an empty string");
    if (doc_id[0] == '_')
        throw runtime_error("_id cannot start with _");

    Json::FastWriter writer;
    string json_doc = writer.write(doc);

    string doc_url(url);
    string *doc_id_escaped = EZ::cURL::escape(doc_id);

    doc_url.append(*doc_id_escaped);
    delete doc_id_escaped;

    string *response;
    auto_ptr<string> response_destroyer;

    try
    {
        response = server.curl.put(doc_url, json_doc);
        response_destroyer.reset(response);
    }
    catch (EZ::HTTPResponse e)
    {
        /* Catch HTTP 409 Resource Conflict */

        if (e.response_code != 409)
            throw;

        throw Conflict(doc_id);
    }

    Json::Reader reader;
    Json::Value info;

    if (!reader.parse(*response, info, false))
        throw runtime_error("JSON Parsing error");

    response_destroyer.reset();

    const Json::Value &new_id = info["id"];
    const Json::Value &new_rev = info["rev"];

    if (!new_id.isString() || !new_rev.isString())
        throw runtime_error("Invalid server response (id, rev !string)");

    if (new_id.asString() != doc_id)
        throw runtime_error("Server has gone insane (saved wrong _id)");

    doc["_rev"] = new_rev;
}

Json::Value *Database::get_doc(const string &doc_id)
{
    string doc_url(url);
    string *doc_id_escaped = EZ::cURL::escape(doc_id);

    doc_url.append(*doc_id_escaped);
    delete doc_id_escaped;

    Json::Reader reader;
    Json::Value *doc = new Json::Value;
    auto_ptr<Json::Value> value_destroyer(doc);

    string *response = server.curl.get(doc_url);
    auto_ptr<string> response_destroyer(response);

    if (!reader.parse(*response, *doc, false))
        throw runtime_error("JSON Parsing error");

    response_destroyer.reset();
    value_destroyer.release();

    return doc;
}

} /* namespace CouchDB */
