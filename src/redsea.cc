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

#include "redsea.h"

#include <iostream>

#include "blockstream.h"

std::string lcd_char(char code) {
  std::vector<std::string> char_map ({
      " ","!","\"","#","¤","%","&","'","(",")","*","+",",","-",".","/",
      "0","1","2","3","4","5","6","7","8","9",":",";","<","=",">","?",
      "@","A","B","C","D","E","F","G","H","I","J","K","L","M","N","O",
      "P","Q","R","S","T","U","V","W","X","Y","Z","[","\\","]","―","_",
      "‖","a","b","c","d","e","f","g","h","i","j","k","l","m","n","o",
      "p","q","r","s","t","u","v","w","x","y","z","{","|","}","¯"," ",
      "á","à","é","è","í","ì","ó","ò","ú","ù","Ñ","Ç","Ş","β","¡","Ĳ",
      "â","ä","ê","ë","î","ï","ô","ö","û","ü","ñ","ç","ş","ǧ","ı","ĳ",
      "ª","α","©","‰","Ǧ","ě","ň","ő","π","€","£","$","←","↑","→","↓",
      "º","¹","²","³","±","İ","ń","ű","µ","¿","÷","°","¼","½","¾","§",
      "Á","À","É","È","Í","Ì","Ó","Ò","Ú","Ù","Ř","Č","Š","Ž","Ð","Ŀ",
      "Â","Ä","Ê","Ë","Î","Ï","Ô","Ö","Û","Ü","ř","č","š","ž","đ","ŀ",
      "Ã","Å","Æ","Œ","ŷ","Ý","Õ","Ø","Þ","Ŋ","Ŕ","Ć","Ś","Ź","Ŧ","ð",
      "ã","å","æ","œ","ŵ","ý","õ","ø","þ","ŋ","ŕ","ć","ś","ź","ŧ"," "});
  return char_map[code - 16];
}

// extract len bits from bitstring, starting at starting_at from the right
uint16_t extract_bits (uint16_t bitstring, int starting_at, int len) {
  return ((bitstring >> starting_at) & ((1<<len) - 1));
}

void Group0A (Group& group, std::vector<uint16_t> blockbits) {
  group.di_address = 3 - extract_bits(blockbits[1], 0, 2);
  group.di = extract_bits(blockbits[1], 2, 1);
  group.ta = extract_bits(blockbits[1], 4, 1);
  group.is_music = extract_bits(blockbits[1], 3, 1);

  if (blockbits.size() < 3)
    return;

  for (int i=0; i<2; i++) {
    int af_code = extract_bits(blockbits[2], 8-i*8, 8);
    if (af_code >= 1 && af_code <= 204) {
      group.altfreqs.push_back(87.5 + af_code / 10.0);
    } else if (af_code == 205) {
      // filler
    } else if (af_code == 224) {
      // no AF exists
    } else if (af_code >= 225 && af_code <= 249) {
      group.num_altfreqs = af_code - 224;
    } else if (af_code == 250) {
      // AM/LF freq follows
    }
  }

  if (blockbits.size() < 4)
    return;

  group.ps_position = extract_bits(blockbits[1], 0, 2) * 2;
  group.ps_chars = lcd_char(extract_bits(blockbits[3], 8, 8)) +
    lcd_char(extract_bits(blockbits[3], 0, 8));

  std::cout << group.ps_chars << "\n";
}

int main() {
  BlockStream block_stream;

  uint16_t pi=0, prev_new_pi=0, new_pi=0;

  while (true) {
    Group group;
    auto blockbits = block_stream.getNextGroup();

    prev_new_pi = new_pi;
    new_pi = blockbits[0];

    if (new_pi == prev_new_pi) {
      pi = new_pi;
      group.pi = pi;

    } else if (new_pi != pi) {
      continue;
    }

    group.type = extract_bits(blockbits[1], 11, 5);
    group.tp   = extract_bits(blockbits[1], 10, 1);
    group.pty  = extract_bits(blockbits[1],  5, 5);

    if (group.type == 0) { Group0A(group, blockbits); }
    /*if (group.type == 1) { Group0B(group, blockbits); }
    if (group.type == 2) { Group1A(group, blockbits); }
    if (group.type == 3) { Group1B(group, blockbits); }
    if (group.type == 4) { Group2A(group, blockbits); }
    if (group.type == 5) { Group2B(group, blockbits); }*/

  }
}
