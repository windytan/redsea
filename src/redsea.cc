/*
 * redsea - RDS decoder
 * Copyright (c) Oona Räisänen OH2EIQ (windyoona@gmail.com)
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

#include <getopt.h>
#include <iostream>

#include "config.h"
#include "block_sync.h"
#include "groups.h"

const std::string kRedseaVersion("0.7.3");

namespace redsea {

void printUsage() {
  printf("radio_command | ./src/redsea [OPTIONS]\n"
         "\n"
         "-b    Input is ASCII bit stream (011010110...)\n"
         "-h    Input is hex groups in the RDS Spy format\n"
         "-x    Output is hex groups in the RDS Spy format\n"
         "-u    Use RBDS (North American) program types\n"
         "-v    Print version\n"
         );
}

void printVersion() {
#ifdef DEBUG
  printf("%s-debug by Oona Raisanen\n", PACKAGE_STRING);
#else
  printf("%s by Oona Raisanen\n", PACKAGE_STRING);
#endif
}

void printShort(Station station) {
    printf("%s 0x%04x %s\n", station.getPS().c_str(), station.getPI(),
        station.getRT().c_str());

  //printf("%04x %2d%s TP:%d PTY:%d\n", station.pi, group.type,
  //    group.type_ab == 1 ? "B" : "A",
  //    group.tp, group.pty);
}

} // namespace redsea

int main(int argc, char** argv) {

  int option_char;
  redsea::eInputType input_type = redsea::INPUT_MPX;
  int output_type = redsea::OUTPUT_JSON;
  bool is_rbds = false;

  while ((option_char = getopt(argc, argv, "bhxvu")) != EOF) {
    switch (option_char) {
      case 'b':
        input_type = redsea::INPUT_ASCIIBITS;
        break;
      case 'h':
        input_type = redsea::INPUT_RDSSPY;
        break;
      case 'x':
        output_type = redsea::OUTPUT_HEX;
        break;
      case 'u':
        is_rbds = true;
        break;
      case 'v':
        redsea::printVersion();
        return 0;
        break;
      case '?':
      default:
        redsea::printUsage();
        return 0;
        break;
    }
  }

#ifndef HAVE_LIQUID
  if (input_type == redsea::INPUT_MPX) {
    printf("can't demodulate MPX: redsea was compiled without liquid-dsp\n");
    return 0;
  }
#endif

  redsea::BlockStream block_stream(input_type);
  redsea::Station station(0, is_rbds);

  uint16_t pi=0, prev_new_pi=0, new_pi=0;

  bool is_eof = false;

  while (!is_eof) {

    redsea::Group group = (input_type == redsea::INPUT_RDSSPY ?
        redsea::getNextGroupRSpy() :
        block_stream.getNextGroup());

    is_eof = group.num_blocks == 0;

    // Repeated PI confirms change
    prev_new_pi = new_pi;
    new_pi = group.block1;

    if (new_pi == prev_new_pi || input_type == redsea::INPUT_RDSSPY) {
      pi = new_pi;
      if (pi != station.getPI())
        station = redsea::Station(pi, is_rbds);
    } else if (new_pi != pi) {
      continue;
    }

#ifdef DEBUG
    printf("b:%f,", block_stream.getT());
#endif

    if (output_type == redsea::OUTPUT_HEX) {
      group.printHex();
    } else {
      station.update(group);
    }

  }
}
