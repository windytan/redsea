redsea
======
This program decodes [RDS](http://en.wikipedia.org/wiki/Radio_Data_System) data received through
a sound card.

The program runs on Linux and uses sox/alsa to read from the sound card. The GUI is written in
Perl and uses GTK2, Encode and IO::Select. Line in must be set on Capture and capture gain should
be reasonably high with no clipping.

![Screenshot](http://www.cs.helsinki.fi/u/okraisan/rds-ylesuomi3.png)

Licensing
---------

    Copyright (c) 2007-2011, windytan (OH2-250)
    
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
