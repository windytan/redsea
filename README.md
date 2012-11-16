redsea
======
This program decodes [RDS](http://en.wikipedia.org/wiki/Radio_Data_System) data from FM broadcasts. It is designed to work with [a special modification](http://windytan.blogspot.fi/2012/10/enchanting-subcarriers-on-fm-part-2.html) to the ATS 909 receiver.

Requires Linux, SoX, and Perl &gt;= 5.10 (with Gtk2 and Encode libraries).

Display
-------

![Screenshot](http://www.cs.helsinki.fi/u/okraisan/radio/redsea-blue.png)

*  big text on the left is Program Service name (PS); text below is RadioText (RT)
*  CL: clock signal detected from IC
*  DT: data signal detected from IC
*  SY: block synchronization acquired
*  RT: receiving RadioText
*  RT+: receiving RadioText+
*  eRT: receiving Enhanced RadioText
*  EON: station broadcasts Enhanced Other Networks information
*  TMC: station broadcasts Traffic Message Channel
*  TP: Traffic Program bit is set
*  TA: Traffic Announcement bit is set
*  PI: Program Identification number (hex)
*  ECC: Extended Country Code
*  PTY: Program Type
*  MHz: station frequency as guessed from AF
*  QUA: quality of received data

Licensing
---------

    Copyright (c) 2007-2012, windytan (OH2-250)
    
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
