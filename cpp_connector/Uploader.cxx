/* Copyright 2011 (C) Daniel Richman. License: GNU GPL 3; see LICENSE. */

#include "Uploader.h"
#include <string>
#include <memory>
#include <stdexcept>
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include "CouchDB.h"
#include "EZ.h"

using namespace std;

namespace habitat {

Uploader::Uploader(const string &callsign, const string &couch_uri,
                   const string &couch_db, int max_merge_attempts)
    : callsign(callsign), server(couch_uri), database(server, couch_db),
      max_merge_attempts(max_merge_attempts)
{
    if (!callsign.length())
        throw invalid_argument("Callsign of zero length");
}

static char hexchar(int n)
{
    if (n < 10)
        return '0' + n;
    else
        return 'a' + n - 10;
}

static string sha256hex(const string &data)
{
    unsigned char hash[SHA256_DIGEST_LENGTH];
    string hexhash;
    hexhash.reserve(SHA256_DIGEST_LENGTH * 2);

    const unsigned char *dc =
        reinterpret_cast<const unsigned char *>(data.c_str());
    SHA256(dc, data.length(), hash);

    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
    {
        char tmp[2];
        tmp[0] = hexchar((hash[i] & 0xF0) >> 4);
        tmp[1] = hexchar(hash[i] & 0x0F);
        hexhash.append(tmp, 2);
    }

    return hexhash;
}

static string base64(const string &data)
{
    /* So it's either this or linking with another b64 library... */
    BIO *bio_mem, *bio_b64;

    bio_b64 = BIO_new(BIO_f_base64());
    bio_mem = BIO_new(BIO_s_mem());
    if (bio_b64 == NULL || bio_mem == NULL)
        throw runtime_error("Base64 conversion failed");

    BIO_set_flags(bio_b64, BIO_FLAGS_BASE64_NO_NL);

    bio_b64 = BIO_push(bio_b64, bio_mem);
    /* Chain is now ->b64->mem */

    size_t result_a;
    int result_b;

    result_a = BIO_write(bio_b64, data.c_str(), data.length());
    result_b = BIO_flush(bio_b64);

    if (result_a != data.length() || result_b != 1)
        throw runtime_error("Base64 conversion failed: BIO_write,flush");

    char *data_b64_c;
    size_t data_b64_length;

    data_b64_length = BIO_get_mem_data(bio_mem, &data_b64_c);
    string data_b64(data_b64_c, data_b64_length);

    BIO_free_all(bio_b64);

    return data_b64;
}

static void set_time(Json::Value &thing, int time_created)
{
    thing["time_uploaded"] = Json::Int(time(NULL));
    thing["time_created"] = time_created;
}

static void payload_telemetry_new(Json::Value &doc,
                                  const string &data_b64,
                                  const string &callsign,
                                  Json::Value &receiver_info)
{
    doc["data"] = Json::Value(Json::objectValue);
    doc["receivers"] = Json::Value(Json::objectValue);
    doc["type"] = "payload_telemetry";

    doc["data"]["_raw"] = data_b64;
    doc["receivers"][callsign] = receiver_info;
}

static void payload_telemetry_merge(Json::Value &doc,
                                    const string &data_b64,
                                    const string &callsign,
                                    Json::Value &receiver_info)
{
    string other_b64 = doc["data"]["_raw"].asString();

    if (!other_b64.length() || other_b64 != data_b64)
        throw CollisionError();

    if (!doc["receivers"].isObject())
        throw runtime_error("Server gave us an invalid payload telemetry doc");

    doc["receivers"][callsign] = receiver_info;
}

string Uploader::payload_telemetry(const string &data,
                                   const Json::Value &metadata,
                                   int time_created)
{
    EZ::MutexLock lock(mutex);

    if (!data.length())
        throw runtime_error("Can't upload string of zero length");

    string doc_id = sha256hex(data);
    string data_b64 = base64(data);

    if (time_created == -1)
        time_created = time(NULL);

    Json::Value receiver_info;

    if (metadata.isObject())
    {
        if (metadata.isMember("time_created") ||
            metadata.isMember("time_uploaded") ||
            metadata.isMember("latest_listener_info") ||
            metadata.isMember("latest_listener_telemetry"))
        {
            throw invalid_argument("found forbidden key in metadata");
        }

        /* This copies metadata. */
        receiver_info = metadata;
    }
    else if (!metadata.isNull())
    {
        throw invalid_argument("metadata must be an object/dict or null");
    }

    if (latest_listener_info.length())
        receiver_info["latest_listener_info"] = latest_listener_info;

    if (latest_listener_telemetry.length())
        receiver_info["latest_listener_telemetry"] = latest_listener_telemetry;

    try
    {
        Json::Value doc(Json::objectValue);
        set_time(receiver_info, time_created);
        payload_telemetry_new(doc, data_b64, callsign, receiver_info);
        doc["_id"] = doc_id;
        database.save_doc(doc);
        return doc_id;
    }
    catch (CouchDB::Conflict e)
    {
        for (int attempts = 0; attempts < max_merge_attempts; attempts++)
        {
            try
            {
                Json::Value *doc = database[doc_id];
                auto_ptr<Json::Value> doc_destroyer(doc);

                set_time(receiver_info, time_created);
                payload_telemetry_merge(*doc, data_b64, callsign,
                                        receiver_info);
                database.save_doc(*doc);

                return doc_id;
            }
            catch (CouchDB::Conflict e)
            {
                continue;
            }
        }

        throw UnmergeableError();
    }
}

string Uploader::listener_doc(const char *type, const Json::Value &data,
                              int time_created)
{
    if (time_created == -1)
        time_created = time(NULL);

    if (!data.isObject())
        throw invalid_argument("data must be an object/dict");

    if (data.isMember("callsign"))
        throw invalid_argument("forbidden key in data");

    Json::Value copied_data(data);
    copied_data["callsign"] = callsign;

    Json::Value doc(Json::objectValue);
    doc["data"] = copied_data;
    doc["type"] = type;

    set_time(doc, time_created);
    database.save_doc(doc);

    return doc["_id"].asString();
}

string Uploader::listener_telemetry(const Json::Value &data, int time_created)
{
    EZ::MutexLock lock(mutex);

    latest_listener_telemetry =
        listener_doc("listener_telemetry", data, time_created);
    return latest_listener_telemetry;
}

string Uploader::listener_info(const Json::Value &data, int time_created)
{
    EZ::MutexLock lock(mutex);

    latest_listener_info =
        listener_doc("listener_info", data, time_created);
    return latest_listener_info;
}

} /* namespace habitat */
