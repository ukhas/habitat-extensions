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

        auto_ptr<Json::Value> doc(d["2aebb7c97381d352bb2986a0b81fd826"]);
        cout << *(doc.get()) << endl;
    }
    catch (EZ::cURLError e)
    {
        cout << "Threw cURLError: " << e << endl;
    }
    catch (EZ::HTTPResponse e)
    {
        cout << "Threw HTTPResponse: " << e << endl;
    }
    catch (const char *e)
    {
        cout << "Threw char*: " << e << endl;
    }

    return 0;
}
