/* Copyright 2013-present Barefoot Networks, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Antonin Bas (antonin@barefootnetworks.com)
 *
 */

#include "bm_sim/parser.h"

template<>
void
ParserOpSet<field_t>::operator()(
  Packet *pkt, const char *data, size_t *bytes_parsed
) const
{
  (void) bytes_parsed; (void) data;
  PHV *phv = pkt->get_phv();
  Field &f_dst = phv->get_field(dst.header, dst.offset);
  Field &f_src = phv->get_field(src.header, src.offset);
  f_dst.set(f_src);
}

template<>
void
ParserOpSet<Data>::operator()(
  Packet *pkt, const char *data, size_t *bytes_parsed
) const
{
  (void) bytes_parsed; (void) data;
  PHV *phv = pkt->get_phv();
  Field &f_dst = phv->get_field(dst.header, dst.offset);
  f_dst.set(src);
}

template<>
void
ParserOpSet<ParserLookAhead>::operator()(
  Packet *pkt, const char *data, size_t *bytes_parsed
) const
{
  (void) bytes_parsed;
  static thread_local ByteContainer bc;
  PHV *phv = pkt->get_phv();
  Field &f_dst = phv->get_field(dst.header, dst.offset);
  int f_bits = f_dst.get_nbits();
  /* I expect the first case to be the most common one. In the first case, we
     extract the packet bytes to the field bytes and sync the bignum value. The
     second case requires extracting the packet bytes to the ByteContainer, then
     importing the bytes into the field's bignum value, then finally exporting
     the bignum value to the field's byte array. I could alternatively write a
     more general extract function which would account for a potential size
     difference between source and destination. */
  // TODO
  if(src.bitwidth == f_bits) {
    data += src.byte_offset;
    f_dst.extract(data, src.bit_offset);
  }
  else {
    bc.clear();
    src.peek(data, bc);
    f_dst.set(bc);
  }
}

bool ParseSwitchCase::match(const ByteContainer &input,
			    const ParseState **state) const {
  if(!with_mask) {
    if(key == input) {
      *state = next_state;
      return true;
    }
  }
  else {
    for(unsigned int byte_index = 0; byte_index < key.size(); byte_index++) {
      if(key[byte_index] != (input[byte_index] & mask[byte_index]))
	return false;
    }
    *state = next_state;
    return true;
  }
  return false;
}

const ParseState *ParseState::operator()(Packet *pkt, const char *data,
					 size_t *bytes_parsed) const{
  // execute parser ops
  PHV *phv = pkt->get_phv();
  for (auto &parser_op : parser_ops)
    (*parser_op)(pkt, data + *bytes_parsed, bytes_parsed);

  if(!has_switch) return NULL;

  // build key
  static thread_local ByteContainer key;
  key.clear();
  key_builder(*phv, data, key);

  // try the matches in order
  const ParseState *next_state = NULL;
  for (const auto &switch_case : parser_switch)
    if(switch_case.match(key, &next_state)) return next_state;

  return default_next_state;
}

void Parser::parse(Packet *pkt) const {
  ELOGGER->parser_start(*pkt, *this);
  const char *data = pkt->data();
  if(!init_state) return;
  const ParseState *next_state = init_state;
  size_t bytes_parsed = 0;
  while(next_state) {
    next_state = (*next_state)(pkt, data, &bytes_parsed);
    // std::cout << "bytes parsed: " << bytes_parsed << std::endl;
  }
  pkt->remove(bytes_parsed);
  ELOGGER->parser_done(*pkt, *this);
}
