/* Copyright 2011 (C) Daniel Richman. License: GNU GPL 3; see COPYING. */

#include "habitat/UKHASExtractor.h"

namespace habitat {

void UKHASExtractor::reset_buffer()
{
    buffer.resize(0);
    buffer.clear();
    buffer.reserve(256);
}

void UKHASExtractor::skipped(int n)
{
    if (n > 3)
        n = 3;

    for (int i = 0; i < n; i++)
        push(0x00);
}

void UKHASExtractor::push(char b, enum push_flags)
{
    if (last == '$' && b == '$')
    {
        /* Start delimiter: "$$" */
        reset_buffer();
        buffer.append(last);
        buffer.append(b);

        garbage_count = 0;
        extracting = true;
        
        mgr->status("UKHAS Extractor: found start delimiter");
    }
    else if (extracting && b == '\n')
    {
        /* End delimiter: "\n" */
        buffer.append(b);
        uthr.payload_telemetry(buffer);

        mgr->status("UKHAS Extractor: extracted string");

        Json::Value data = crude_parse(buffer);

        if (!data->isNull())
            mgr->data(data);
    }
    else if (extracting)
    {
        buffer.append(b);

        if (b < 0x20 || b > 0x7E)
            garbage_count++;

        /* Sane limits to avoid uploading tonnes of garbage */
        if (buffer.length() > 1024 || garbage_count > 16)
        {
            reset_buffer();
            extracting = false;
        }
    }

    last = b;
}

} /* namespace habitat */
