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

#include <stack>

#include <iostream>

#include <cassert>

#include "bm_sim/conditionals.h"
#include "bm_sim/phv.h"

ExprOpcodesMap::ExprOpcodesMap() {
  opcodes_map = {
    {"load_field", ExprOpcode::LOAD_FIELD},
    {"load_header", ExprOpcode::LOAD_HEADER},
    {"load_bool", ExprOpcode::LOAD_BOOL},
    {"load_const", ExprOpcode::LOAD_CONST},
    {"+", ExprOpcode::ADD},
    {"==", ExprOpcode::EQ_DATA},
    {"!=", ExprOpcode::NEQ_DATA},
    {">", ExprOpcode::GT_DATA},
    {"<", ExprOpcode::LT_DATA},
    {">=", ExprOpcode::GET_DATA},
    {"<=", ExprOpcode::LET_DATA},
    {"and", ExprOpcode::AND},
    {"or", ExprOpcode::OR},
    {"not", ExprOpcode::NOT},
    {"&", ExprOpcode::BIT_AND},
    {"|", ExprOpcode::BIT_OR},
    {"^", ExprOpcode::BIT_XOR},
    {"~", ExprOpcode::BIT_NEG},
    {"valid", ExprOpcode::VALID_HEADER},
  };
}

ExprOpcodesMap *ExprOpcodesMap::get_instance() {
  static ExprOpcodesMap instance;
  return &instance;
}

ExprOpcode ExprOpcodesMap::get_opcode(std::string expr_name) {
  ExprOpcodesMap *instance = get_instance();
  return instance->opcodes_map[expr_name];
}

void Conditional::push_back_load_field(header_id_t header, int field_offset) {
  Op op;
  op.opcode = ExprOpcode::LOAD_FIELD;
  op.field = {header, field_offset};
  ops.push_back(op);
}

void Conditional::push_back_load_bool(bool value) {
  Op op;
  op.opcode = ExprOpcode::LOAD_BOOL;
  op.bool_value = value;
  ops.push_back(op);
}

void Conditional::push_back_load_header(header_id_t header) {
  Op op;
  op.opcode = ExprOpcode::LOAD_HEADER;
  op.header = header;
  ops.push_back(op);
}

void Conditional::push_back_load_const(const Data &data) {
  const_values.push_back(data);
  Op op;
  op.opcode = ExprOpcode::LOAD_CONST;
  op.const_offset = const_values.size() - 1;
  ops.push_back(op);
}

void Conditional::push_back_op(ExprOpcode opcode) {
  Op op;
  op.opcode = opcode;
  ops.push_back(op);
}

void Conditional::build() {
  data_registers_cnt = assign_dest_registers();
  built = true;
}


/* I have made this function more efficient by using thread_local variables
   instead of dynamic allocation at each call. Maybe it would be better to just
   try to use a stack allocator */
bool Conditional::eval(const PHV &phv) const {
  assert(built);

  static thread_local int data_temps_size = 4;
  // std::vector<Data> data_temps(data_registers_cnt);
  static thread_local std::vector<Data> data_temps(data_temps_size);
  while(data_temps_size < data_registers_cnt) {
    data_temps.emplace_back();
    data_temps_size++;
  }

  /* Logically, I am using these as stacks but experiments showed that using
     vectors directly was more efficient (also I can call reserve to avoid 
     multiple calls to malloc */

  /* 4 is arbitrary, it is possible to do an analysis on the Conditional to find
     the exact number needed, but I don't think it is worth it... */

  static thread_local std::vector<bool> bool_temps_stack;
  bool_temps_stack.clear();
  // bool_temps_stack.reserve(4);

  static thread_local std::vector<const Data *> data_temps_stack;
  data_temps_stack.clear();
  // data_temps_stack.reserve(4);

  static thread_local std::vector<const Header *> header_temps_stack;
  header_temps_stack.clear();
  // header_temps_stack.reserve(4);

  bool lb, rb;
  const Data *ld, *rd;

  for(const auto &op : ops) {
    switch(op.opcode) {
    case ExprOpcode::LOAD_FIELD:
      data_temps_stack.push_back(&(phv.get_field(op.field.header,
						 op.field.field_offset)));
      break;

    case ExprOpcode::LOAD_HEADER:
      header_temps_stack.push_back(&(phv.get_header(op.header)));
      break;

    case ExprOpcode::LOAD_BOOL:
      bool_temps_stack.push_back(op.bool_value);
      break;

    case ExprOpcode::LOAD_CONST:
      data_temps_stack.push_back(&const_values[op.const_offset]);
      break;

    case ExprOpcode::ADD:
      rd = data_temps_stack.back(); data_temps_stack.pop_back();
      ld = data_temps_stack.back(); data_temps_stack.pop_back();
      data_temps[op.data_dest_index].add(*ld, *rd);
      data_temps_stack.push_back(&data_temps[op.data_dest_index]);
      break;

    case ExprOpcode::EQ_DATA:
      rd = data_temps_stack.back(); data_temps_stack.pop_back();
      ld = data_temps_stack.back(); data_temps_stack.pop_back();
      bool_temps_stack.push_back(*ld == *rd);
      break;

    case ExprOpcode::NEQ_DATA:
      rd = data_temps_stack.back(); data_temps_stack.pop_back();
      ld = data_temps_stack.back(); data_temps_stack.pop_back();
      bool_temps_stack.push_back(*ld != *rd);
      break;

    case ExprOpcode::GT_DATA:
      rd = data_temps_stack.back(); data_temps_stack.pop_back();
      ld = data_temps_stack.back(); data_temps_stack.pop_back();
      bool_temps_stack.push_back(*ld > *rd);
      break;

    case ExprOpcode::LT_DATA:
      rd = data_temps_stack.back(); data_temps_stack.pop_back();
      ld = data_temps_stack.back(); data_temps_stack.pop_back();
      bool_temps_stack.push_back(*ld < *rd);
      break;

    case ExprOpcode::GET_DATA:
      rd = data_temps_stack.back(); data_temps_stack.pop_back();
      ld = data_temps_stack.back(); data_temps_stack.pop_back();
      bool_temps_stack.push_back(*ld >= *rd);
      break;

    case ExprOpcode::LET_DATA:
      rd = data_temps_stack.back(); data_temps_stack.pop_back();
      ld = data_temps_stack.back(); data_temps_stack.pop_back();
      bool_temps_stack.push_back(*ld <= *rd);
      break;

    case ExprOpcode::AND:
      rb = bool_temps_stack.back(); bool_temps_stack.pop_back();
      lb = bool_temps_stack.back(); bool_temps_stack.pop_back();
      bool_temps_stack.push_back(lb && rb);
      break;

    case ExprOpcode::OR:
      rb = bool_temps_stack.back(); bool_temps_stack.pop_back();
      lb = bool_temps_stack.back(); bool_temps_stack.pop_back();
      bool_temps_stack.push_back(lb || rb);
      break;

    case ExprOpcode::NOT:
      rb = bool_temps_stack.back(); bool_temps_stack.pop_back();
      bool_temps_stack.push_back(!rb);
      break;

    case ExprOpcode::BIT_AND:
      rd = data_temps_stack.back(); data_temps_stack.pop_back();
      ld = data_temps_stack.back(); data_temps_stack.pop_back();
      data_temps[op.data_dest_index].bit_and(*ld, *rd);
      data_temps_stack.push_back(&data_temps[op.data_dest_index]);
      break;

    case ExprOpcode::BIT_OR:
      rd = data_temps_stack.back(); data_temps_stack.pop_back();
      ld = data_temps_stack.back(); data_temps_stack.pop_back();
      data_temps[op.data_dest_index].bit_or(*ld, *rd);
      data_temps_stack.push_back(&data_temps[op.data_dest_index]);
      break;

    case ExprOpcode::BIT_XOR:
      rd = data_temps_stack.back(); data_temps_stack.pop_back();
      ld = data_temps_stack.back(); data_temps_stack.pop_back();
      data_temps[op.data_dest_index].bit_xor(*ld, *rd);
      data_temps_stack.push_back(&data_temps[op.data_dest_index]);
      break;

    case ExprOpcode::BIT_NEG:
      rd = data_temps_stack.back(); data_temps_stack.pop_back();
      data_temps[op.data_dest_index].bit_neg(*rd);
      data_temps_stack.push_back(&data_temps[op.data_dest_index]);
      break;

    case ExprOpcode::VALID_HEADER:
      bool_temps_stack.push_back(header_temps_stack.back()->is_valid());
      header_temps_stack.pop_back();
      break;

    default:
      break;
    }
  }

  return bool_temps_stack.back();
}

const ControlFlowNode *Conditional::operator()(Packet *pkt) const
{
  PHV *phv = pkt->get_phv();
  bool result = eval(*phv);
  ELOGGER->condition_eval(*pkt, *this, result);
  return result ? true_next : false_next;
}

int Conditional::assign_dest_registers() {
  int registers_cnt = 0;
  std::stack<int> new_registers;
  for(auto &op : ops) {
    switch(op.opcode) {
    case ExprOpcode::ADD:
    case ExprOpcode::SUB:
    case ExprOpcode::MOD:
    case ExprOpcode::BIT_AND:
    case ExprOpcode::BIT_OR:
    case ExprOpcode::BIT_XOR:
      registers_cnt -= new_registers.top();
      new_registers.pop();
      registers_cnt -= new_registers.top();
      new_registers.pop();

      op.data_dest_index = registers_cnt;

      new_registers.push(1);
      registers_cnt += 1;
      break;

    case ExprOpcode::BIT_NEG:
      registers_cnt -= new_registers.top();
      new_registers.pop();

      op.data_dest_index = registers_cnt;

      new_registers.push(1);
      registers_cnt += 1;
      break;

    case ExprOpcode::LOAD_CONST:
    case ExprOpcode::LOAD_FIELD:
      new_registers.push(0);
      break;
      
    default:
      break;
    }
  }
  return registers_cnt;
}
