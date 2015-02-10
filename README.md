redsea
======
redsea decodes [RDS](http://en.wikipedia.org/wiki/Radio_Data_System) data from FM broadcasts. It works with rtl_fm.

[explanatory blog post](http://www.windytan.com/2015/02/receiving-rds-with-rtl-sdr.html)

Requirements
------------

* Linux or OSX
* [rtl-sdr](http://sdr.osmocom.org/trac/wiki/rtl-sdr)
* [SoX](http://sox.sourceforge.net/)
* Perl

Compiling
---------

    gcc -std=gnu99 -o rtl_redsea rtl_redsea.c -lm

Usage
-----

    perl redsea.pl -f 94.0M

        -f FREQ  station frequency in Hz, can be SI prefixed
                 (e.g. 94.0M)

Licensing
---------

    Copyright (c) 2007-2015, Oona Räisänen (OH2EIQ)
    
    Permission to use, copy, modify, and/or distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.
    
    THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
    MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
