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

 /*! @file
  *  This is an example of the PIN tool that demonstrates some basic PIN APIs
  *  and could serve as the starting point for developing your first PIN tool
  */

#include "pin.H"
#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <string.h>
#include <string>

#include "../../include/ChampSim/trace_instruction.h"

using trace_instr_format_t = input_instr;

#if (GC_TRACE==ENABLE)

// #define GC_START "signal_gc_start"
#define GC_START "GC_stopped_mark"
#define GC_MARK_END "GC_apply_to_all_blocks_for_reclaim_block" 
#define GC_SWEEP_END "GC_start_reclaim"           

/* ================================================================== */
// Global variables 
/* ================================================================== */

UINT64 instrCount = 0;

UINT64 pre_instrCount = 0;

UINT64 pre_instrCount_for_mark_end = 0;

std::ofstream outfile;

trace_instr_format_t curr_instr;

/* ===================================================================== */
// Command line switches
/* ===================================================================== */
KNOB<std::string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "champsim.trace",
                                 "specify file name for Champsim tracer output");

KNOB<UINT64> KnobSkipInstructions(KNOB_MODE_WRITEONCE, "pintool", "s", "0",
                                  "How many instructions to skip before tracing begins");

KNOB<UINT64> KnobTraceInstructions(KNOB_MODE_WRITEONCE, "pintool", "t", "1000000",
                                   "How many instructions to trace");

/* ===================================================================== */
// Utilities
/* ===================================================================== */

/*!
 *  Print out help message.
 */
INT32 Usage()
{
  std::cerr << "This tool creates a register and memory access trace" << std::endl
    << "Specify the output trace file with -o" << std::endl
    << "Specify the number of instructions to skip before tracing with -s" << std::endl
    << "Specify the number of instructions to trace with -t" << std::endl << std::endl;

  std::cerr << KNOB_BASE::StringKnobSummary() << std::endl;

  return -1;
}

/* ===================================================================== */
// Analysis routines
/* ===================================================================== */

void ResetCurrentInstruction(VOID* ip)
{
  curr_instr = {};
  curr_instr.ip = (unsigned long long int)ip;
}
BOOL ShouldWrite()
{
  ++instrCount;
  return (instrCount > KnobSkipInstructions.Value()) && (instrCount <= (KnobTraceInstructions.Value() + KnobSkipInstructions.Value()));
}

void WriteCurrentInstruction()
{
  typename decltype(outfile)::char_type buf[sizeof(trace_instr_format_t)];
  std::memcpy(buf, &curr_instr, sizeof(trace_instr_format_t));
  outfile.write(buf, sizeof(trace_instr_format_t)); //curr_instrのデータをbufに渡す。curr_instrには.ip, .is_branchなどの値がある。

  // std::cout << "=======================================================" << std::endl;
  // std::cout << "=======================================================" << std::endl;
  // std::cout << "============(WriteCurrentInstruction)============" << std::endl;
  // static int writecount=0;
  // if(writecount >= 10000) {
  //    std::cout << "(WriteCurrentInstruction)" << std::endl;
  //    writecount = 0;
  // }
  // else{
  //   writecount++;
  // }
  // if(curr_instr.is_branch==1) {
  //   std::cout << "(WriteCurrentInstruction) curr_instr.is_branch==1" << std::endl;
  // }
  if(curr_instr.is_gc_rtn_start==1) {
    std::cout << "(WriteCurrentInstruction) curr_instr.is_gc_rtn_start==1" << std::endl;
  }
  if(curr_instr.is_gc_rtn_end==1) {
    std::cout << "(WriteCurrentInstruction) curr_instr.is_gc_rtn_end==1" << std::endl;
  }
  if(curr_instr.is_mark_end!=0) {
    std::cout << "(WriteCurrentInstruction) curr_instr.is_mark_end!=0" << std::endl;
  }
  if(curr_instr.is_gc_rtn_sweep_end==1) {
    std::cout << "(WriteCurrentInstruction) curr_instr.is_gc_rtn_sweep_end==1" << std::endl;
  }
  // taiga debug
}

void BranchOrNot(UINT32 taken)
{
  curr_instr.is_branch = 1;
  curr_instr.branch_taken = taken;
}

template <typename T>
void WriteToSet(T* begin, T* end, UINT32 r)
{
  auto set_end = std::find(begin, end, 0);
  auto found_reg = std::find(begin, set_end, r); // check to see if this register is already in the list
  *found_reg = r; //すでに存在しているレジスタならそのまま、存在していなければ末尾に追加。
}

/* ===================================================================== */
// Print routine function
/* ===================================================================== */
VOID Print_rtn_start(CHAR* name)
{
  std::cout << "====================" << "Print_rtn_start" << "====================" << std::endl;
  curr_instr.is_gc_rtn_start = 1;
  
  // taiga debug
  UINT64 diff_instrCount = instrCount - pre_instrCount;
  pre_instrCount = instrCount;
  std::cout << "Diff instruction(Print_rtn_start) " << diff_instrCount << std::endl;
  // taiga debug

}

VOID Print_rtn_end(CHAR* name)
{
  std::cout << "====================" << "Print_rtn_end" << "====================" << std::endl;
  curr_instr.is_gc_rtn_end = 1;
  // std::cout << "==================curr_instr.is_gc_rtn_end " << curr_instr.is_gc_rtn_end << "============" << std::endl;
}

VOID Print_rtn_mark_end(CHAR* name)
{
  std::cout << "====================" << " is_mark_end " << name << "====================" << std::endl;
  curr_instr.is_mark_end = 1;
  // std::cout << "==================curr_instr.is_mark_end " << curr_instr.is_mark_end << std::endl;

  // taiga debug
  UINT64 diff_instrCount_for_mark_end = instrCount - pre_instrCount_for_mark_end;
  pre_instrCount_for_mark_end = instrCount;
  std::cout << "Diff instruction(Print_rtn_mark_end) " << diff_instrCount_for_mark_end << std::endl;
  // taiga debug
}

VOID Print_rtn_sweep_end(CHAR* name)
{
  std::cout << "====================" << "Print_rtn_sweep_end" << "====================" << std::endl;
  curr_instr.is_gc_rtn_sweep_end = 1;
  // std::cout << "==================curr_instr.is_gc_rtn_end " << curr_instr.is_gc_rtn_end << "============" << std::endl;
}

// taiga debug
VOID Print_inscount() {
  std::cout << "instruction count = " << instrCount << std::endl; 
}
// taiga debug

/* ===================================================================== */
// Instrumentation callbacks　ver Taiga
/* ===================================================================== */


VOID Image(IMG img, VOID* v)
{
  for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec))
  {
    for (RTN rtn = SEC_RtnHead(sec); RTN_Valid(rtn); rtn = RTN_Next(rtn))
    {

      // taiga debug
      bool rtn_main = false;
      // taiga debug

      bool rtn_is_gc = false;
      bool rtn_is_gc_start = false;
      bool rtn_is_mark_end = false;
      bool rtn_is_sweep = false;
      // Prepare for processing of RTN, an  RTN is not broken up into BBLs,
      // it is merely a sequence of INSs 
      RTN_Open(rtn);

      // taiga debug
      if(RTN_Name(rtn) == "main") {
        rtn_main = true;
      }
      // taiga debug

      // GC_START rtn
      if(RTN_Name(rtn) == GC_START) {
        rtn_is_gc = true;
        rtn_is_gc_start = true;
      }
      if(RTN_Name(rtn) == GC_MARK_END) {
        rtn_is_mark_end = true;
      }

      if(RTN_Name(rtn) == GC_SWEEP_END) {
        rtn_is_sweep = true;
      }
            
      for (INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins))
      {
        // begin each instruction with this function
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)ResetCurrentInstruction, IARG_INST_PTR, IARG_END);

        // instrument branch instructions
        if (INS_IsBranch(ins))
          INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)BranchOrNot, IARG_BRANCH_TAKEN, IARG_END);

        // instrument register reads
        UINT32 readRegCount = INS_MaxNumRRegs(ins);
        for (UINT32 i = 0; i < readRegCount; i++)
        {
          UINT32 regNum = INS_RegR(ins, i);
          INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)WriteToSet<unsigned char>,
                        IARG_PTR, curr_instr.source_registers, IARG_PTR, curr_instr.source_registers + NUM_INSTR_SOURCES,
                        IARG_UINT32, regNum, IARG_END);
        }

        // instrument register writes
        UINT32 writeRegCount = INS_MaxNumWRegs(ins);
        for (UINT32 i = 0; i < writeRegCount; i++)
        {
          UINT32 regNum = INS_RegW(ins, i);
          INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)WriteToSet<unsigned char>,
                        IARG_PTR, curr_instr.destination_registers, IARG_PTR, curr_instr.destination_registers + NUM_INSTR_DESTINATIONS,
                        IARG_UINT32, regNum, IARG_END);
        }

        // instrument memory reads and writes
        UINT32 memOperands = INS_MemoryOperandCount(ins);

        // Iterate over each memory operand of the instruction.
        for (UINT32 memOp = 0; memOp < memOperands; memOp++)
        {
          if (INS_MemoryOperandIsRead(ins, memOp))
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)WriteToSet<unsigned long long int>,
                          IARG_PTR, curr_instr.source_memory, IARG_PTR, curr_instr.source_memory + NUM_INSTR_SOURCES,
                          IARG_MEMORYOP_EA, memOp, IARG_END);
          if (INS_MemoryOperandIsWritten(ins, memOp))
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)WriteToSet<unsigned long long int>,
                          IARG_PTR, curr_instr.destination_memory, IARG_PTR, curr_instr.destination_memory + NUM_INSTR_DESTINATIONS,
                          IARG_MEMORYOP_EA, memOp, IARG_END);
        }

        // taiga debug
        if(rtn_main) {
          if(INS_IsRet(ins)) {
            std::cout << "Finded main return" << std::endl;
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)Print_inscount, IARG_END);
            rtn_main = false;
          }
        }
        // taiga debug

        if (rtn_is_gc == true) {
          // GC_STARTの開始なら
          if(rtn_is_gc_start == true) {
            // taiga debug
            std::cout << "====================" << "Finded GC_start_rtn " << GC_START << "====================" << std::endl;
            // taiga debug
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)Print_rtn_start, IARG_ADDRINT, GC_START, IARG_END);
            rtn_is_gc_start = false;
          }
          // GC_ENDなら
          if(INS_IsRet(ins))
          {
            // taiga debug
            std::cout << "====================" << "Finded GC_end_rtn " << GC_START << "====================" << std::endl;
            // taiga debug
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)Print_rtn_end, IARG_ADDRINT, GC_START, IARG_END);
            rtn_is_gc = false;
          }
        }
        // MARK_ENDなら（正常に動いていない可能性がある）
        if(rtn_is_mark_end == true) {
          // taiga debug
          std::cout << "====================" << "Finded mark_end " << GC_MARK_END << "====================" << std::endl;
          // taiga debug
          INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)Print_rtn_mark_end, IARG_ADDRINT, GC_MARK_END, IARG_END);
          rtn_is_mark_end = false;
        }
        // Sweep_ENDなら
        if (rtn_is_sweep == true) {
          if(INS_IsRet(ins))
          {
            // taiga debug
            std::cout << "====================" << "Finded GC_sweep_end " << GC_SWEEP_END << "====================" << std::endl;
            // taiga debug
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)Print_rtn_sweep_end, IARG_ADDRINT, GC_SWEEP_END, IARG_END);
            rtn_is_sweep = false;
          }
        }
        // finalize each instruction with this function
        INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)ShouldWrite, IARG_END);
        INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)WriteCurrentInstruction, IARG_END);
      }
      RTN_Close(rtn);
    }
  }
}
/*!
 * Print out analysis results.
 * This function is called when the application exits.
 * @param[in]   code            exit code of the application
 * @param[in]   v               value specified by the tool in the
 *                              PIN_AddFiniFunction function call
 */
VOID Fini(INT32 code, VOID* v)
{
  outfile.close();
}

/*!
 * The main procedure of the tool.
 * This function is called when the application image is loaded but not yet started.
 * @param[in]   argc            total number of elements in the argv array
 * @param[in]   argv            array of command line arguments,
 *                              including pin -t <toolname> -- ...
 */
int main(int argc, char* argv[])
{

  // === for print routine name ===
  // Initialize symbol table code, needed for rtn instrumentation
  PIN_InitSymbols();
  // === for print routine name ===

  // Initialize PIN library. Print help message if -h(elp) is specified
  // in the command line or the command line is invalid 
  if (PIN_Init(argc, argv))
    return Usage();

  outfile.open(KnobOutputFile.Value().c_str(), std::ios_base::binary | std::ios_base::trunc);
  if (!outfile)
  {
    std::cout << "Couldn't open output trace file. Exiting." << std::endl;
    exit(1);
  }
  // プリントファンクション
  // Register function to be called to instrument instructions
  IMG_AddInstrumentFunction(Image, 0);

  // Register function to be called when the application exits
  PIN_AddFiniFunction(Fini, 0);

  // Start the program, never returns
  // このプログラムからは戻ってこない。アプリケーションの実行を開始する。
  PIN_StartProgram();

  return 0;
}

#else //GC_TRACE

/* ================================================================== */
// Global variables 
/* ================================================================== */

UINT64 instrCount = 0;

std::ofstream outfile;

trace_instr_format_t curr_instr;

/* ===================================================================== */
// Command line switches
/* ===================================================================== */
KNOB<std::string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "champsim.trace",
                                 "specify file name for Champsim tracer output");

KNOB<UINT64> KnobSkipInstructions(KNOB_MODE_WRITEONCE, "pintool", "s", "0",
                                  "How many instructions to skip before tracing begins");

KNOB<UINT64> KnobTraceInstructions(KNOB_MODE_WRITEONCE, "pintool", "t", "1000000",
                                   "How many instructions to trace");

/* ===================================================================== */
// Utilities
/* ===================================================================== */

/*!
 *  Print out help message.
 */
INT32 Usage()
{
  std::cerr << "This tool creates a register and memory access trace" << std::endl
    << "Specify the output trace file with -o" << std::endl
    << "Specify the number of instructions to skip before tracing with -s" << std::endl
    << "Specify the number of instructions to trace with -t" << std::endl << std::endl;

  std::cerr << KNOB_BASE::StringKnobSummary() << std::endl;

  return -1;
}
/* ===================================================================== */
// Analysis routines
/* ===================================================================== */

void ResetCurrentInstruction(VOID* ip)
{
  curr_instr = {};
  curr_instr.ip = (unsigned long long int)ip;
}

BOOL ShouldWrite()
{
    
  ++instrCount;
  // taiga debug
  if(instrCount % 100000 == 0) {
    std::cout << "instrCount = " << instrCount << std::endl;
  }
  // taiga debug
  return (instrCount > KnobSkipInstructions.Value()) && (instrCount <= (KnobTraceInstructions.Value() + KnobSkipInstructions.Value()));
}

void WriteCurrentInstruction()
{
  typename decltype(outfile)::char_type buf[sizeof(trace_instr_format_t)];
  std::memcpy(buf, &curr_instr, sizeof(trace_instr_format_t));
  outfile.write(buf, sizeof(trace_instr_format_t)); //curr_instrのデータをbufに渡す。curr_instrには.ip, .is_branchなどの値がある。

}

void BranchOrNot(UINT32 taken)
{
  curr_instr.is_branch = 1;
  curr_instr.branch_taken = taken;
}

template <typename T>
void WriteToSet(T* begin, T* end, UINT32 r)
{
  auto set_end = std::find(begin, end, 0);
  auto found_reg = std::find(begin, set_end, r); // check to see if this register is already in the list
  *found_reg = r; //すでに存在しているレジスタならそのまま、存在していなければ末尾に追加。
}

// taiga debug
VOID Print_inscount() {
  std::cout << "instruction count = " << instrCount << std::endl; 
}
// taiga debug

/* ===================================================================== */
// Instrumentation callbacks
/* ===================================================================== */

// Is called for every instruction and instruments reads and writes
VOID Instruction(INS ins, VOID* v)
{
  // begin each instruction with this function
  INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)ResetCurrentInstruction, IARG_INST_PTR, IARG_END);

  // instrument branch instructions
  if (INS_IsBranch(ins))
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)BranchOrNot, IARG_BRANCH_TAKEN, IARG_END);

  // instrument register reads
  UINT32 readRegCount = INS_MaxNumRRegs(ins);
  for (UINT32 i = 0; i < readRegCount; i++)
  {
    UINT32 regNum = INS_RegR(ins, i);
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)WriteToSet<unsigned char>,
                   IARG_PTR, curr_instr.source_registers, IARG_PTR, curr_instr.source_registers + NUM_INSTR_SOURCES,
                   IARG_UINT32, regNum, IARG_END);
  }

  // instrument register writes
  UINT32 writeRegCount = INS_MaxNumWRegs(ins);
  for (UINT32 i = 0; i < writeRegCount; i++)
  {
    UINT32 regNum = INS_RegW(ins, i);
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)WriteToSet<unsigned char>,
                   IARG_PTR, curr_instr.destination_registers, IARG_PTR, curr_instr.destination_registers + NUM_INSTR_DESTINATIONS,
                   IARG_UINT32, regNum, IARG_END);
  }

  // instrument memory reads and writes
  UINT32 memOperands = INS_MemoryOperandCount(ins);

  // Iterate over each memory operand of the instruction.
  for (UINT32 memOp = 0; memOp < memOperands; memOp++)
  {
    if (INS_MemoryOperandIsRead(ins, memOp))
      INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)WriteToSet<unsigned long long int>,
                     IARG_PTR, curr_instr.source_memory, IARG_PTR, curr_instr.source_memory + NUM_INSTR_SOURCES,
                     IARG_MEMORYOP_EA, memOp, IARG_END);
    if (INS_MemoryOperandIsWritten(ins, memOp))
      INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)WriteToSet<unsigned long long int>,
                     IARG_PTR, curr_instr.destination_memory, IARG_PTR, curr_instr.destination_memory + NUM_INSTR_DESTINATIONS,
                     IARG_MEMORYOP_EA, memOp, IARG_END);
  }

  // finalize each instruction with this function
  INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)ShouldWrite, IARG_END);
  INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)WriteCurrentInstruction, IARG_END);
}
/*!
 * Print out analysis results.
 * This function is called when the application exits.
 * @param[in]   code            exit code of the application
 * @param[in]   v               value specified by the tool in the
 *                              PIN_AddFiniFunction function call
 */
VOID Fini(INT32 code, VOID* v)
{
  outfile.close();
}

/*!
 * The main procedure of the tool.
 * This function is called when the application image is loaded but not yet started.
 * @param[in]   argc            total number of elements in the argv array
 * @param[in]   argv            array of command line arguments,
 *                              including pin -t <toolname> -- ...
 */
int main(int argc, char* argv[])
{

  // Initialize PIN library. Print help message if -h(elp) is specified
  // in the command line or the command line is invalid 
  if (PIN_Init(argc, argv))
    return Usage();

  outfile.open(KnobOutputFile.Value().c_str(), std::ios_base::binary | std::ios_base::trunc);
  if (!outfile)
  {
    std::cout << "Couldn't open output trace file. Exiting." << std::endl;
    exit(1);
  }

  // Register function to be called to instrument instructions
  INS_AddInstrumentFunction(Instruction, 0);

  // Register function to be called when the application exits
  PIN_AddFiniFunction(Fini, 0);

  // Start the program, never returns
  // このプログラムからは戻ってこない。アプリケーションの実行を開始する。
  PIN_StartProgram();

  return 0;
}

#endif //GC_TRACE