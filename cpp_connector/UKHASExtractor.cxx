/* Copyright 2011 (C) Daniel Richman. License: GNU GPL 3; see COPYING. */

#include "UKHASExtractor.h"
#include <json/json.h>

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
        push('\0', PUSH_NONE);
}

void UKHASExtractor::push(char b, enum push_flags flags)
{
    if (last == '$' && b == '$')
    {
        /* Start delimiter: "$$" */
        reset_buffer();
        buffer.push_back(last);
        buffer.push_back(b);

        garbage_count = 0;
        extracting = true;
        
        mgr->status("UKHAS Extractor: found start delimiter");
    }
    else if (extracting && b == '\n')
    {
        /* End delimiter: "\n" */
        buffer.push_back(b);
        mgr->uthr.payload_telemetry(buffer);

        mgr->status("UKHAS Extractor: extracted string");

        Json::Value data = crude_parse();
        if (!data.isNull())
            mgr->data(data);
    }
    else if (extracting)
    {
        /* baudot doesn't support '*', so we use '#'. */
        if ((flags & PUSH_BAUDOT_HACK) && b == '#')
            b = '*';

        buffer.push_back(b);

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

Json::Value UKHASExtractor::crude_parse()
{
    /* Get flight doc from mgr */
    /* TODO crude parse */
    return Json::Value::null;
}

} /* namespace habitat */
