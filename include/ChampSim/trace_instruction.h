/*
 *    Copyright 2023 The ChampSim Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef TRACE_INSTRUCTION_H
#define TRACE_INSTRUCTION_H

#include <limits>
#include "/home/funkytaiga/tmp_champ/ChampSim-Ramulator/include/ProjectConfiguration.h"

// special registers that help us identify branches
namespace champsim
{
constexpr char REG_STACK_POINTER       = 6;
constexpr char REG_FLAGS               = 25;
constexpr char REG_INSTRUCTION_POINTER = 26;
} // namespace champsim

// instruction format
constexpr std::size_t NUM_INSTR_DESTINATIONS_SPARC = 4;
constexpr std::size_t NUM_INSTR_DESTINATIONS       = 2;
constexpr std::size_t NUM_INSTR_SOURCES            = 4;

struct input_instr
{
    // instruction pointer or PC (Program Counter)
    unsigned long long ip;

    // branch info
    unsigned char is_branch;
    unsigned char branch_taken;

    unsigned char destination_registers[NUM_INSTR_DESTINATIONS]; // output registers
    unsigned char source_registers[NUM_INSTR_SOURCES];           // input registers

    unsigned long long destination_memory[NUM_INSTR_DESTINATIONS]; // output memory
    unsigned long long source_memory[NUM_INSTR_SOURCES];           // input memory

    // taiga added
    // 関数の開始と終了、およびその名前を格納
#if (GC_TRACE == ENABLE)
    unsigned char is_gc_rtn_start;
    unsigned char is_gc_rtn_end;
    unsigned char is_mark_end; //マークが終了。tmp.txtからmarked addressを読み出す。
    unsigned char is_gc_rtn_sweep_end;
    // char function_name[256];
#endif
};

struct cloudsuite_instr
{
    // instruction pointer or PC (Program Counter)
    unsigned long long ip;

    // branch info
    unsigned char is_branch;
    unsigned char branch_taken;

    unsigned char destination_registers[NUM_INSTR_DESTINATIONS_SPARC]; // output registers
    unsigned char source_registers[NUM_INSTR_SOURCES];                 // input registers

    unsigned long long destination_memory[NUM_INSTR_DESTINATIONS_SPARC]; // output memory
    unsigned long long source_memory[NUM_INSTR_SOURCES];                 // input memory

    unsigned char asid[2];

    // taiga added
    // 関数の開始と終了、およびその名前を格納
#if (GC_TRACE == ENABLE)
    unsigned char is_gc_rtn_start;
    unsigned char is_gc_rtn_end;
    unsigned char is_mark_end; //マークが終了。tmp.txtからmarked addressを読み出す。
    unsigned char is_gc_rtn_sweep_end;
    // char function_name[256];
#endif
};

#endif
