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
#include "src/common.h"
#include "src/block_sync.h"
#include "src/groups.h"

namespace redsea {

void printUsage() {
  std::cout <<
         "radio_command | ./src/redsea [OPTIONS]\n"
         "\n"
         "-b|--input-ascii  Input is ASCII bit stream (011010110...)\n"
         "-e|--feed-through Echo the input signal to stdout and print decoded\n"
         "                  groups to stderr\n"
         "-h|--input-hex    Input is hex groups in the RDS Spy format\n"
         "-x|--output-hex   Output is hex groups in the RDS Spy format\n"
         "-u|--rbds         Use RBDS (North American) program types\n"
         "-v|--version      Print version\n";
}

void printVersion() {
#ifdef DEBUG
  std::cout << PACKAGE_STRING << "-debug by Oona Raisanen" << std::endl;
#else
  std::cout << PACKAGE_STRING << " by Oona Raisanen" << std::endl;
#endif
}

Options getOptions(int argc, char** argv) {
  redsea::Options options;

  static struct option long_options[] = {
    { "input-binary",  no_argument, 0, 'b'},
    { "feed-through",  no_argument, 0, 'e'},
    { "input-hex",     no_argument, 0, 'h'},
    { "output-hex",    no_argument, 0, 'x'},
    { "show-partial",  no_argument, 0, 'p'},
    { "rbds",          no_argument, 0, 'u'},
    { "help",          no_argument, 0, '?'},
    { "version",       no_argument, 0, 'v'},
    {0,                0,           0,  0}};

  int option_index = 0;
  int option_char;

  while ((option_char = getopt_long(argc, argv, "behxvu?", long_options,
         &option_index)) >= 0) {
    switch (option_char) {
      case 'b':
        options.input_type = redsea::INPUT_ASCIIBITS;
        break;
      case 'e':
        options.feed_thru = true;
        break;
      case 'h':
        options.input_type = redsea::INPUT_RDSSPY;
        break;
      case 'x':
        options.output_type = redsea::OUTPUT_HEX;
        break;
      case 'p':
        options.show_partial = true;
        break;
      case 'u':
        options.rbds = true;
        break;
      case 'v':
        redsea::printVersion();
        options.just_exit = true;
        break;
      case '?':
      default:
        redsea::printUsage();
        options.just_exit = true;
        break;
    }
  }

  return options;
}

}  // namespace redsea

int main(int argc, char** argv) {

  redsea::Options options = redsea::getOptions(argc, argv);

  if (options.just_exit)
    exit(EXIT_SUCCESS);

#ifndef HAVE_LIQUID
  if (options.input_type == redsea::INPUT_MPX) {
    std::cerr << "can't demodulate MPX: redsea was compiled without liquid-dsp"
              << std::endl;
    return 0;
  }
#endif

  redsea::BlockStream block_stream(options);
  redsea::Station station(0, options);

  uint16_t pi = 0, prev_new_pi = 0, new_pi = 0;

  bool is_eof = false;

  // Line buffering
  if (options.feed_thru)
    setvbuf(stderr, NULL, _IOLBF, 2048);
  else
    setvbuf(stdout, NULL, _IOLBF, 2048);

  while (!is_eof) {
    redsea::Group group = (options.input_type == redsea::INPUT_RDSSPY ?
        redsea::getNextGroupRSpy(options.feed_thru) :
        block_stream.getNextGroup());

    is_eof = (std::cin.eof() || block_stream.isEOF());

    if (group.hasPi) {
      // Repeated PI confirms change
      prev_new_pi = new_pi;
      new_pi = group.block[redsea::OFFSET_A];

      if (new_pi == prev_new_pi || options.input_type == redsea::INPUT_RDSSPY) {
        pi = new_pi;
        if (pi != station.getPI())
          station = redsea::Station(pi, options);
      } else if (new_pi != pi) {
        continue;
      }
    }

#ifdef DEBUG
    printf("b:%f,", block_stream.getT());
#endif

    if (options.output_type == redsea::OUTPUT_HEX) {
      group.printHex(options.feed_thru ? &std::cerr : &std::cout);
    } else {
      station.updateAndPrint(group, options.feed_thru ?
                             &std::cerr : &std::cout);
    }
  }
}
