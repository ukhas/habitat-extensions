#include <iostream>
#include "CouchDB.h"

using namespace std;
using namespace CouchDB;

int main(int argc, char **argv)
{
    try
    {
        Server s("http://localhost:5984/");
        //Database d(s, "habitat");
        cout << s.next_uuid() << endl;
    }
    catch (cURLError e)
    {
        cout << "Threw cURLError: " << e << endl;
    }
    catch (const char *e)
    {
        cout << "Threw char*: " << e << endl;
    }
    return 0;
}
