/* Copyright 2011 (C) Daniel Richman. License: GNU GPL 3; see LICENSE. */

#include <iostream>
#include <memory>

#include "EZ.h"
#include "CouchDB.h"

using namespace std;

int main(int argc, char **argv)
{
    EZ::cURLGlobal cgl;

    try
    {
        CouchDB::Server s("http://localhost:5984/");
        CouchDB::Database d = s["habitat"];

        // auto_ptr<Json::Value> doc(d["2aebb7c97381d352bb2986a0b81fd826"]);
        // cout << *(doc.get()) << endl;

        Json::Value doc(Json::objectValue);
        doc["testing"] = true;
        doc["hello"] = Json::Value(Json::arrayValue);
        doc["hello"].append("World");
        doc["hello"].append("From");
        doc["hello"].append("c++");

        d.save_doc(doc);
        cout << doc << endl;
    }
    catch (EZ::cURLError e)
    {
        cout << "Threw cURLError: " << e << endl;
    }
    catch (EZ::HTTPResponse e)
    {
        cout << "Threw HTTPResponse: " << e << endl;
    }
    catch (CouchDB::Conflict e)
    {
        cout << "Threw Conflict: " << e << endl;
    }
    catch (const char *e)
    {
        cout << "Threw char*: " << e << endl;
    }

    return 0;
}
