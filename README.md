# TOI utilities

## Pre-requisites

This depends on a header only library, [futile](https://github.com/rmarianski/futile). The [futile.h](https://github.com/rmarianski/futile/blob/master/futile.h) include file is expected to be on your include path.

You will also need the hiredis library.

## Scripts

It is separated out into 3 commands:

1. toi

This allows you to view counts by zoom for the tiles of interest.

To build:

    make

First you'll want to save the toi onto a local file:

    ./toi save -f toi.bin -h <redis-host>

Next, you can use it to print out stats about it:

    ./toi print -f toi.bin

2. toi-diff

This takes in a coordinate range (or several), and will display counts per zoom of tiles that are missing from the tiles of interest.

To build:

    make toi-diff

The range specifier should be in tile coordinates. `minx,miny,maxx,maxy:minz-maxz`. For example:

    ./toi-diff -f toi.bin 313,703,469,759:11-14

3. toi-log

This gives us an idea of how many tiles of interest would be pruned at particular zoom levels. It operates in 2 modes, first it creates a binary file of the log entries, and then it compares the log entries with the tiles of interest. A code change is required to update an #if in the main function to toggle which mode this is running in.

To build:

    make toi-log

It expects a text file of sql results in the format `z | x | y | n`. A csv would probably have been better.

    ./toi-log sql-results.txt

This will generate a file `log_entries.bin`. Now, make the code change to switch the #if to process the log entries, and rebuild. Now, simply running it will print out the new toi counts by zoom after pruning for the given request count.
