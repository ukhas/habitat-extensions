#include <iostream>

#include "EZ.h"
#include "CouchDB.h"

using namespace std;

int main(int argc, char **argv)
{
    EZ::cURLGlobal cgl;

    try
    {
        CouchDB::Server s("http://localhost:5984/");
        // Database d(s, "habitat");
        cout << s.next_uuid() << endl;
    }
    catch (EZ::cURLError e)
    {
        cout << "Threw cURLError: " << e << endl;
    }
    catch (const char *e)
    {
        cout << "Threw char*: " << e << endl;
    }

    return 0;
}
