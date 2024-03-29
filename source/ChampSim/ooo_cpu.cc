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

#include "ChampSim/ooo_cpu.h"

#include "ProjectConfiguration.h" // User file

#if (USE_VCPKG == ENABLE)
#include <fmt/chrono.h>
#include <fmt/core.h>
#include <fmt/ranges.h>
#endif // USE_VCPKG

#include <algorithm>
#include <chrono>
#include <cmath>
#include <numeric>

#include "ChampSim/cache.h"
#include "ChampSim/champsim.h"
#include "ChampSim/deadlock.h"
#include "ChampSim/instruction.h"
#include "ChampSim/util/span.h"
#include "/home/funkytaiga/tools/gc-8.2.2/include/for_champsim.h"
#include "history_based_page_selection.h"


std::chrono::seconds elapsed_time();

long O3_CPU::operate()
{
    long progress {0};

    progress += retire_rob();                    // retire
    progress += complete_inflight_instruction(); // finalize execution
    progress += execute_instruction();           // execute instructions
    progress += schedule_instruction();          // schedule instructions
    progress += handle_memory_return();          // finalize memory transactions
    progress += operate_lsq();                   // execute memory transactions

    progress += dispatch_instruction(); // dispatch
    progress += decode_instruction();   // decode
    progress += promote_to_decode();

    progress += fetch_instruction(); // fetch
    progress += check_dib();
    initialize_instruction();

    // heartbeat
    if (show_heartbeat && (num_retired >= next_print_instruction))
    {
#if (USE_VCPKG == ENABLE)
        auto heartbeat_instr {std::ceil(num_retired - last_heartbeat_instr)};
        auto heartbeat_cycle {std::ceil(current_cycle - last_heartbeat_cycle)};

        auto phase_instr {std::ceil(num_retired - begin_phase_instr)};
        auto phase_cycle {std::ceil(current_cycle - begin_phase_cycle)};

        fmt::print("Heartbeat CPU {} instructions: {} cycles: {} heartbeat IPC: {:.4g} cumulative IPC: {:.4g} (Simulation time: {:%H hr %M min %S sec})\n", cpu,
            num_retired, current_cycle, heartbeat_instr / heartbeat_cycle, phase_instr / phase_cycle, elapsed_time());

#endif // USE_VCPKG

        next_print_instruction += STAT_PRINTING_PERIOD;

        last_heartbeat_instr = num_retired;
        last_heartbeat_cycle = current_cycle;
    }

    return progress;
}

void O3_CPU::initialize()
{
    // BRANCH PREDICTOR & BTB
    impl_initialize_branch_predictor();
    impl_initialize_btb();
}

void O3_CPU::begin_phase()
{
    begin_phase_instr = num_retired;
    begin_phase_cycle = current_cycle;

    // Record where the next phase begins
    stats_type stats;
    stats.name         = "CPU " + std::to_string(cpu);
    stats.begin_instrs = num_retired;
    stats.begin_cycles = current_cycle;
    sim_stats          = stats;
}

void O3_CPU::end_phase(unsigned finished_cpu)
{
    // Record where the phase ended (overwrite if this is later)
    sim_stats.end_instrs = num_retired;
    sim_stats.end_cycles = current_cycle;

    if (finished_cpu == this->cpu)
    {
        finish_phase_instr = num_retired;
        finish_phase_cycle = current_cycle;

        roi_stats          = sim_stats;
    }
}

void O3_CPU::initialize_instruction()
{
    auto instrs_to_read_this_cycle = std::min(FETCH_WIDTH, static_cast<long>(IFETCH_BUFFER_SIZE - std::size(IFETCH_BUFFER)));

    while (current_cycle >= fetch_resume_cycle && instrs_to_read_this_cycle > 0 && ! std::empty(input_queue))
    {
        instrs_to_read_this_cycle--;

        auto stop_fetch = do_init_instruction(input_queue.front());
        if (stop_fetch)
            instrs_to_read_this_cycle = 0;

        // Add to IFETCH_BUFFER
        IFETCH_BUFFER.push_back(input_queue.front());
        input_queue.pop_front();

        IFETCH_BUFFER.back().event_cycle = current_cycle;
    }
}

namespace
{
void do_stack_pointer_folding(ooo_model_instr& arch_instr)
{
    // The exact, true value of the stack pointer for any given instruction can usually be determined immediately after the instruction is decoded without
    // waiting for the stack pointer's dependency chain to be resolved.
    bool writes_sp = std::count(std::begin(arch_instr.destination_registers), std::end(arch_instr.destination_registers), champsim::REG_STACK_POINTER);
    if (writes_sp)
    {
        // Avoid creating register dependencies on the stack pointer for calls, returns, pushes, and pops, but not for variable-sized changes in the
        // stack pointer position. reads_other indicates that the stack pointer is being changed by a variable amount, which can't be determined before
        // execution.
        bool reads_other = std::count_if(std::begin(arch_instr.source_registers), std::end(arch_instr.source_registers), [](uint8_t r)
            { return r != champsim::REG_STACK_POINTER && r != champsim::REG_FLAGS && r != champsim::REG_INSTRUCTION_POINTER; });
        if ((arch_instr.is_branch != 0) || ! (std::empty(arch_instr.destination_memory) && std::empty(arch_instr.source_memory)) || (! reads_other))
        {
            auto nonsp_end = std::remove(std::begin(arch_instr.destination_registers), std::end(arch_instr.destination_registers), champsim::REG_STACK_POINTER);
            arch_instr.destination_registers.erase(nonsp_end, std::end(arch_instr.destination_registers));
        }
    }
}
} // namespace

bool O3_CPU::do_predict_branch(ooo_model_instr& arch_instr)
{
    bool stop_fetch = false;

    // handle branch prediction for all instructions as at this point we do not know if the instruction is a branch
    sim_stats.total_branch_types[arch_instr.branch_type]++;
    auto [predicted_branch_target, always_taken] = impl_btb_prediction(arch_instr.ip);
    arch_instr.branch_prediction                 = impl_predict_branch(arch_instr.ip) || always_taken;
    if (arch_instr.branch_prediction == 0)
        predicted_branch_target = 0;

    if (arch_instr.is_branch)
    {
        if constexpr (champsim::debug_print)
        {
#if (USE_VCPKG == ENABLE)
            fmt::print("[BRANCH] instr_id: {} ip: {:#x} taken: {}\n", arch_instr.instr_id, arch_instr.ip, arch_instr.branch_taken);
#endif // USE_VCPKG
        }

        // call code prefetcher every time the branch predictor is used
        l1i->impl_prefetcher_branch_operate(arch_instr.ip, arch_instr.branch_type, predicted_branch_target);

        if (predicted_branch_target != arch_instr.branch_target || (((arch_instr.branch_type == BRANCH_CONDITIONAL) || (arch_instr.branch_type == BRANCH_OTHER)) && arch_instr.branch_taken != arch_instr.branch_prediction))
        { // conditional branches are re-evaluated at decode when the target is computed
            sim_stats.total_rob_occupancy_at_branch_mispredict += std::size(ROB);
            sim_stats.branch_type_misses[arch_instr.branch_type]++;
            if (! warmup)
            {
                fetch_resume_cycle             = std::numeric_limits<uint64_t>::max();
                stop_fetch                     = true;
                arch_instr.branch_mispredicted = 1;
            }
        }
        else
        {
            stop_fetch = arch_instr.branch_taken; // if correctly predicted taken, then we can't fetch anymore instructions this cycle
        }

        impl_update_btb(arch_instr.ip, arch_instr.branch_target, arch_instr.branch_taken, arch_instr.branch_type);
        impl_last_branch_result(arch_instr.ip, arch_instr.branch_target, arch_instr.branch_taken, arch_instr.branch_type);
    }

    return stop_fetch;
}

bool O3_CPU::do_init_instruction(ooo_model_instr& arch_instr)
{
    // fast warmup eliminates register dependencies between instructions branch predictor, cache contents, and prefetchers are still warmed up
    if (warmup)
    {
        arch_instr.source_registers.clear();
        arch_instr.destination_registers.clear();
    }

    ::do_stack_pointer_folding(arch_instr);
    return do_predict_branch(arch_instr);
}

long O3_CPU::check_dib()
{
    // scan through IFETCH_BUFFER to find instructions that hit in the decoded instruction buffer
    auto begin                      = std::find_if(std::begin(IFETCH_BUFFER), std::end(IFETCH_BUFFER), [](const ooo_model_instr& x)
                             { return ! x.dib_checked; });
    auto [window_begin, window_end] = champsim::get_span(begin, std::end(IFETCH_BUFFER), FETCH_WIDTH);
    std::for_each(window_begin, window_end, [this](auto& ifetch_entry)
        { this->do_check_dib(ifetch_entry); });
    return std::distance(window_begin, window_end);
}

void O3_CPU::do_check_dib(ooo_model_instr& instr)
{
    // Check DIB to see if we recently fetched this line
    if (auto dib_result = DIB.check_hit(instr.ip); dib_result)
    {
        // The cache line is in the L0, so we can mark this as complete
        instr.fetched     = COMPLETED;

        // Also mark it as decoded
        instr.decoded     = COMPLETED;

        // It can be acted on immediately
        instr.event_cycle = current_cycle;
    }

    instr.dib_checked = COMPLETED;
}

long O3_CPU::fetch_instruction()
{
    long progress {0};

    // Fetch a single cache line
    auto fetch_ready = [](const ooo_model_instr& x)
    {
        return x.dib_checked == COMPLETED && ! x.fetched;
    };

    // Find the chunk of instructions in the block
    auto no_match_ip = [](const auto& lhs, const auto& rhs)
    {
        return (lhs.ip >> LOG2_BLOCK_SIZE) != (rhs.ip >> LOG2_BLOCK_SIZE);
    };

    auto l1i_req_begin = std::find_if(std::begin(IFETCH_BUFFER), std::end(IFETCH_BUFFER), fetch_ready);
    for (auto to_read = L1I_BANDWIDTH; to_read > 0 && l1i_req_begin != std::end(IFETCH_BUFFER); --to_read)
    {
        auto l1i_req_end = std::adjacent_find(l1i_req_begin, std::end(IFETCH_BUFFER), no_match_ip);
        if (l1i_req_end != std::end(IFETCH_BUFFER))
            l1i_req_end = std::next(l1i_req_end); // adjacent_find returns the first of the non-equal elements

        // Issue to L1I
        auto success = do_fetch_instruction(l1i_req_begin, l1i_req_end);
        if (success)
        {
            std::for_each(l1i_req_begin, l1i_req_end, [](auto& x)
                { x.fetched = INFLIGHT; });
            ++progress;
        }

        l1i_req_begin = std::find_if(l1i_req_end, std::end(IFETCH_BUFFER), fetch_ready);
    }

    return progress;
}

bool O3_CPU::do_fetch_instruction(std::deque<ooo_model_instr>::iterator begin, std::deque<ooo_model_instr>::iterator end)
{
    CacheBus::request_type fetch_packet;
    fetch_packet.v_address          = begin->ip;
    fetch_packet.instr_id           = begin->instr_id;
    fetch_packet.ip                 = begin->ip;
    fetch_packet.instr_depend_on_me = {begin, end};

    if constexpr (champsim::debug_print)
    {
#if (USE_VCPKG == ENABLE)
        fmt::print("[IFETCH] {} instr_id: {} ip: {:#x} dependents: {} event_cycle: {}\n", __func__, begin->instr_id, begin->ip,
            std::size(fetch_packet.instr_depend_on_me), begin->event_cycle);
#endif // USE_VCPKG
    }

    return L1I_bus.issue_read(fetch_packet);
}

long O3_CPU::promote_to_decode()
{
    auto available_fetch_bandwidth  = std::min<long>(FETCH_WIDTH, DECODE_BUFFER_SIZE - std::size(DECODE_BUFFER));
    auto [window_begin, window_end] = champsim::get_span_p(std::begin(IFETCH_BUFFER), std::end(IFETCH_BUFFER), available_fetch_bandwidth,
        [cycle = current_cycle](const auto& x)
        { return x.fetched == COMPLETED && x.event_cycle <= cycle; });
    long progress {std::distance(window_begin, window_end)};

    std::for_each(window_begin, window_end,
        [cycle = current_cycle, lat = DECODE_LATENCY, warmup = warmup](auto& x)
        { return x.event_cycle = cycle + ((warmup || x.decoded) ? 0 : lat); });
    std::move(window_begin, window_end, std::back_inserter(DECODE_BUFFER));
    IFETCH_BUFFER.erase(window_begin, window_end);

    return progress;
}

long O3_CPU::decode_instruction()
{
    auto available_decode_bandwidth = std::min<long>(DECODE_WIDTH, DISPATCH_BUFFER_SIZE - std::size(DISPATCH_BUFFER));
    auto [window_begin, window_end] = champsim::get_span_p(std::begin(DECODE_BUFFER), std::end(DECODE_BUFFER), available_decode_bandwidth,
        [cycle = current_cycle](const auto& x)
        { return x.event_cycle <= cycle; });
    long progress {std::distance(window_begin, window_end)};

    // Send decoded instructions to dispatch
    std::for_each(window_begin, window_end, [&, this](auto& db_entry)
        {
    this->do_dib_update(db_entry);

    // Resume fetch
    if (db_entry.branch_mispredicted) {
      // These branches detect the misprediction at decode
      if ((db_entry.branch_type == BRANCH_DIRECT_JUMP) || (db_entry.branch_type == BRANCH_DIRECT_CALL)
          || (((db_entry.branch_type == BRANCH_CONDITIONAL) || (db_entry.branch_type == BRANCH_OTHER)) && db_entry.branch_taken == db_entry.branch_prediction)) {
        // clear the branch_mispredicted bit so we don't attempt to resume fetch again at execute
        db_entry.branch_mispredicted = 0;
        // pay misprediction penalty
        this->fetch_resume_cycle = this->current_cycle + BRANCH_MISPREDICT_PENALTY;
      }
    }

    // Add to dispatch
    db_entry.event_cycle = this->current_cycle + (this->warmup ? 0 : this->DISPATCH_LATENCY); });

    std::move(window_begin, window_end, std::back_inserter(DISPATCH_BUFFER));
    DECODE_BUFFER.erase(window_begin, window_end);

    return progress;
}

void O3_CPU::do_dib_update(const ooo_model_instr& instr) { DIB.fill(instr.ip); }

long O3_CPU::dispatch_instruction()
{
    auto available_dispatch_bandwidth = DISPATCH_WIDTH;

    // dispatch DISPATCH_WIDTH instructions into the ROB
    while (available_dispatch_bandwidth > 0 && ! std::empty(DISPATCH_BUFFER) && DISPATCH_BUFFER.front().event_cycle < current_cycle && std::size(ROB) != ROB_SIZE && ((std::size_t) std::count_if(std::begin(LQ), std::end(LQ), [](const auto& lq_entry)
                                                                                                                                                                          { return ! lq_entry.has_value(); }) >= std::size(DISPATCH_BUFFER.front().source_memory)) &&
           ((std::size(DISPATCH_BUFFER.front().destination_memory) + std::size(SQ)) <= SQ_SIZE))
    {
        ROB.push_back(std::move(DISPATCH_BUFFER.front()));
        DISPATCH_BUFFER.pop_front();
        do_memory_scheduling(ROB.back());

        available_dispatch_bandwidth--;
    }

    return DISPATCH_WIDTH - available_dispatch_bandwidth;
}

long O3_CPU::schedule_instruction()
{
    auto search_bw = SCHEDULER_SIZE;
    int progress {0};
    for (auto rob_it = std::begin(ROB); rob_it != std::end(ROB) && search_bw > 0; ++rob_it)
    {
        if (rob_it->scheduled == 0)
        {
            do_scheduling(*rob_it);
            ++progress;
        }

        if (rob_it->executed == 0)
            --search_bw;
    }

    return progress;
}

void O3_CPU::do_scheduling(ooo_model_instr& instr)
{
    // Mark register dependencies
    for (auto src_reg : instr.source_registers)
    {
        if (! std::empty(reg_producers[src_reg]))
        {
            ooo_model_instr& prior = reg_producers[src_reg].back();
            if (prior.registers_instrs_depend_on_me.empty() || prior.registers_instrs_depend_on_me.back().get().instr_id != instr.instr_id)
            {
                prior.registers_instrs_depend_on_me.push_back(instr);
                instr.num_reg_dependent++;
            }
        }
    }

    for (auto dreg : instr.destination_registers)
    {
        auto begin = std::begin(reg_producers[dreg]);
        auto end   = std::end(reg_producers[dreg]);
        auto ins   = std::lower_bound(begin, end, instr, [](const ooo_model_instr& lhs, const ooo_model_instr& rhs)
              { return lhs.instr_id < rhs.instr_id; });
        reg_producers[dreg].insert(ins, std::ref(instr));
    }

    instr.scheduled   = COMPLETED;
    instr.event_cycle = current_cycle + (warmup ? 0 : SCHEDULING_LATENCY);
}

long O3_CPU::execute_instruction()
{
    auto exec_bw = EXEC_WIDTH;
    for (auto rob_it = std::begin(ROB); rob_it != std::end(ROB) && exec_bw > 0; ++rob_it)
    {
        if (rob_it->scheduled == COMPLETED && rob_it->executed == 0 && rob_it->num_reg_dependent == 0 && rob_it->event_cycle <= current_cycle)
        {
            do_execution(*rob_it);
        }
    }

    return EXEC_WIDTH - exec_bw;
}

void O3_CPU::do_execution(ooo_model_instr& rob_entry)
{
    rob_entry.executed    = INFLIGHT;
    rob_entry.event_cycle = current_cycle + (warmup ? 0 : EXEC_LATENCY);

    // Mark LQ entries as ready to translate
    for (auto& lq_entry : LQ)
        if (lq_entry.has_value() && lq_entry->instr_id == rob_entry.instr_id){
            lq_entry->event_cycle = current_cycle + (warmup ? 0 : EXEC_LATENCY);
#if (PRINT_V_ADDRESS == ENABLE) // for debug
            // print_lq_v_address(rob_entry);
#endif // PRINT_V_ADDRESS
        }
            
    // Mark SQ entries as ready to translate
    for (auto& sq_entry : SQ)
        if (sq_entry.instr_id == rob_entry.instr_id) {
            sq_entry.event_cycle = current_cycle + (warmup ? 0 : EXEC_LATENCY);
#if (PRINT_V_ADDRESS == ENABLE) // debug
            // print_sq_v_address(rob_entry);
#endif // PRINT_V_ADDRESS
        }
        
    if constexpr (champsim::debug_print)
    {
#if (USE_VCPKG == ENABLE)
        fmt::print("[ROB] {} instr_id: {} event_cycle: {}\n", __func__, rob_entry.instr_id, rob_entry.event_cycle);
#endif // USE_VCPKG
    }
// taiga added
#if (GC_MIGRATION_WITH_GC == ENABLE && NO_MIGRATION == DISABLE)
    
    static bool migration_d_is_short = false;
    if(rob_entry.is_gc_rtn_start == 1) {
        // degug
        if(num_retired - os_transparent_management->pre_migration_instr <= DISTANCE_MIGRATION) {
            // std::cout << "is_gc_rtn_start == 1(ooo_cpu.cc)" << std::endl;
            // std::cout << "マイグレーション間隔が短いのでマイグレーションを行いません。" << std::endl;
            migration_d_is_short = true;
            return;
        }
        else {
            // std::cout << "is_gc_rtn_start == 1(ooo_cpu.cc)" << std::endl;
        }
        // degug
        

        // hotness_data_block_address_queueをクリアする
        while(!os_transparent_management->hotness_data_block_address_queue.empty()) {
            os_transparent_management->hotness_data_block_address_queue.pop();
        }
        // remapping_request_queueをクリアする
        while(!os_transparent_management->remapping_request_queue.empty()) {
            os_transparent_management->remapping_request_queue.pop_front();
        }
#if (GC_MARKED_OBJECT == ENABLE)
        std::vector<uint64_t> marked_pages = find_marked_pages();
        std::thread thread3([this, marked_pages] {migration_with_gc_count = migration_with_gc(marked_pages, os_transparent_management); });
        thread3.join();
#else // GC_MARKED_OBJECT
        std::vector<uint64_t> unmarked_pages = find_marked_pages(); // find_unmarked_pages()
        std::thread thread2([this, unmarked_pages] {migration_with_gc_count = migration_with_gc(unmarked_pages, os_transparent_management); });
        thread2.join();
#endif // GC_MARKED_OBJECT
        // カウンターテーブルをクリア
        // CLEAR_COUNTER_TABLE_EPOCH_NUMエポック毎に行う
        os_transparent_management->initialize_counter_table(os_transparent_management->counter_table);

        os_transparent_management->pre_migration_instr = num_retired;

        // GC中のマイグレーションにかかったサイクル数を計算
        migration_with_gc_cycle = OVERHEAD_OF_MIGRATION_PER_PAGE * migration_with_gc_count;
        migration_with_gc_tlb_cycle = OVERHEAD_OF_TLB_SHOOTDOWN_PER_PAGE * migration_with_gc_count;
        // uint64_t migration_cycles = memory_controller->migration_with_gc(marked_pages);
        
        // debug
        std::cout << "migration_with_gc_count " << migration_with_gc_count << std::endl;
        std::cout << "migration_with_gc_cycle " << migration_with_gc_cycle << std::endl; 
        std::cout << "migration_with_gc_tlb_cycle " << migration_with_gc_tlb_cycle << std::endl; 
        // debug
        os_transparent_management->gcmigration_tlb_overhead += migration_with_gc_tlb_cycle;
        // GC_start時のcurrent_cycleを記録
        gc_start_cycle = current_cycle;
        gc_start_ins = num_retired;
    }
    if(rob_entry.is_gc_rtn_end == 1) {
        // degug
        if(migration_d_is_short) {
            std::cout << "is_gc_rtn_end == 1(ooo_cpu.cc)" << std::endl;
            std::cout << "マイグレーションは行っていません。" << std::endl;
            migration_d_is_short = false;
            return; // 終了
        }
        else {
            std::cout << "is_gc_rtn_end == 1(ooo_cpu.cc)" << std::endl;
            // degug
            // サイクル数を計算して修正
            gc_end_cycle = current_cycle;
            gc_end_ins = num_retired;
            gc_cycle = gc_end_cycle - gc_start_cycle;
            gc_ins = gc_end_ins - gc_start_ins;
            // check
            if(gc_cycle <= 0) {
                std::cout << "ERROR:gc_cycleが不正な値です" << std::endl;
                abort();
            }

            // taiga debug
            // std::cout << "gc_cycles " << gc_cycle << std::endl;
            // std::cout << "gc_instructions " << gc_ins << std::endl;
            // taiga debug

            // サイクル数の調整
            if(migration_with_gc_cycle > gc_cycle) {
                // current_cycle = gc_start_cycle + migration_with_gc_cycle;
                // debug
                uint64_t diff_gcmigration = migration_with_gc_cycle-gc_cycle;
                std::cout << "GC migrationの方が時間がかかっています" << std::endl;
                std::cout << "migration_with_gc_cycle - gc_cycle = " << diff_gcmigration << std::endl;
                // debug
                os_transparent_management->gcmigration_sum_overhead_without_tlb += diff_gcmigration;
            }
            // elseは何もしない

            // TLBのオーバーヘッドを追加（最後にやることにしました）
            // current_cycle += migration_with_gc_tlb_cycle;
        }
    }
    if(rob_entry.is_gc_rtn_sweep_end == 1) {
        // degug
        std::cout << "is_gc_rtn_sweep_end == 1(ooo_cpu.cc)" << std::endl;
        // degug
    }
}

std::string O3_CPU::marked_page_file_name = "";

std::vector<uint64_t> O3_CPU::find_marked_pages()
{
    static int gc_count = 1;
    std::vector<uint64_t> p_marked_pages = {}, v_marked_pages = {};
    uint64_t v_page_start_address, v_page_end_address;
    uint64_t v_page_start_page, v_page_end_page;
    bool start_end_flag = false; //start address : false, end address : true;
    
#if (GC_MARKED_OBJECT == ENABLE)
    bool is_there_any_marked_pages = false;
#else
    std::vector<uint64_t> p_unmarked_pages = {}, v_unmarked_pages = {};
    bool is_there_any_unmarked_pages = false;
#endif
    // taiga debug
    std::cout << "marked_page_file_name.xz : " << marked_page_file_name << " (ooo_cpu.h)" << std::endl; 
    // taiga debug
    // ファイルストリームを開く
    std::ifstream file_stream(marked_page_file_name);
    // ファイルが正しく開かれたかを確認
    if (!file_stream.is_open()) {
        std::cerr << "ファイルを開くことができませんでした: " << marked_page_file_name << std::endl;
        abort();
    }
    // ファイルからデータを読み込む
    std::string line, pre_line;
    bool read_flag = false;
    bool full_gc_flag = false;

    bool twice_repeat = false; // for error check
#if (GC_MARKED_OBJECT == ENABLE)
    while (std::getline(file_stream, line)) {
        if(gc_count == 0) break; //最初のGCは参照するmarked_pagesがない
        // std::cout << line << std::endl; // 読み込んだ行を表示
        int gc_count_line; //gc_countをintに変換
        if('0' <= line[0] && line[0] <= '9') {
            gc_count_line = std::stoi(line);
        }
        else {
            gc_count_line = 999999999; //ありえない値を挿入
        }

        if(gc_count_line == gc_count) { //対象のマークページを検出
            read_flag = true;
            std::getline(file_stream, line);
            if(line != "GC_start") {
                std::cout << "ERROR:marked pageのファイルが正しくありません" << std::endl;
                std::cout << "gc_countの次はGC_STARTです" << std::endl;
                abort();
            }
            // error check
            // full gc check
            if(gc_count != 1) {
                if(pre_line == "FULL_GC_START") {
                    std::cout << "full gc(ooo_cpu.h)" << std::endl;
                    full_gc_flag = true;
                    break; //full gc なら終了
                }
            }

            continue;
        }
        if(read_flag) {
            std::string check_start_end = line.substr(0,6);
            if(check_start_end == "sta_ad") {
                is_there_any_marked_pages = true; //marked pageがあります
                // error check
                if(start_end_flag != false) {
                    std::cout << "ERROR:marked pageのファイルが正しくありません" << std::endl;
                    abort();
                }

                std::string marked_start_address_hex = line.substr(7,20);
                try {
                    size_t pos; //デバッグ用
                    v_page_start_address = std::stoull(marked_start_address_hex, &pos, 16);

                    // pos が文字列の長さと一致することを確認（余分な文字がないことを確認）
                    if (pos == marked_start_address_hex.length()) {
                        // 何もしない
                    } else {
                        std::cerr << "変換失敗: 不正な文字が存在します" << std::endl;
                        exit(1);
                    }
                } catch (const std::invalid_argument& e) {
                    std::cerr << "変換失敗: 無効な引数です" << std::endl;
                    exit(1);
                } catch (const std::out_of_range& e) {
                    std::cerr << "変換失敗: 範囲外です" << std::endl;
                    exit(1);
                }

                start_end_flag = true; //次はend address
            }
            else if(check_start_end == "end_ad") {
                // error check
                if(start_end_flag == false) {
                    std::cout << "ERROR:marked pageのファイルが正しくありません" << std::endl;
                    abort();
                }

                // end addressを取得
                std::string marked_end_address_hex = line.substr(7,20);
                try {
                    size_t pos; //デバッグ用
                    v_page_end_address = std::stoull(marked_end_address_hex, &pos, 16);

                    // pos が文字列の長さと一致することを確認（余分な文字がないことを確認）
                    if (pos == marked_end_address_hex.length()) {
                        // std::cout << "変換成功: " << v_page_end_address << std::endl;
                        // 何もしない
                    } else {
                        std::cerr << "変換失敗: 不正な文字が存在します" << std::endl;
                        exit(1);
                    }
                    } catch (const std::invalid_argument& e) {
                    std::cerr << "変換失敗: 無効な引数です" << std::endl;
                    exit(1);
                } catch (const std::out_of_range& e) {
                    std::cerr << "変換失敗: 範囲外です" << std::endl;
                    exit(1);
                }

                // v_marked_pagesにマークされたページを追加
                v_page_start_page = v_page_start_address >> LOG2_PAGE_SIZE;
                v_page_end_page = v_page_end_address >> LOG2_PAGE_SIZE;
                // debug
                // std::cout << "v_page_start_page " << v_page_start_page << std::endl;
                // std::cout << "v_page_end_page " << v_page_end_page << std::endl;

                for (uint64_t i = v_page_start_page; i <= v_page_end_page; i++) {
                    v_marked_pages.push_back(i);
                }

                start_end_flag = false; //次はstart address
            }
            else if(check_start_end == "GC_end") {
                break;
            }
            else {
                std::cout << "ERROR:marked pageのファイルが正しくありません" << std::endl;
                abort();
            }
        }
        pre_line = line;

        // 終端に到達したら折り返し
        if(file_stream.eof()) {
            // エラーチェック
            if(twice_repeat) {
                std::cout << "ERROR:(un)marked pagesの読み込みが行われていません。折り返しが二回目です。" << std::endl;
                std::cout << "gc count " << gc_count << std::endl;
                abort();
            }
            gc_count = 0; //gc_countをリセット
            file_stream.seekg(0, std::ios::beg); //ファイルの先頭に戻る

            twice_repeat = true; // for error check
        }
    }

#else // GC_MARKED_OBJECT
    while (std::getline(file_stream, line)) {
        if(read_flag == false) {
            int gc_count_line; //gc_countをintに変換
            if('0' <= line[0] && line[0] <= '9') {
                gc_count_line = std::stoi(line);
            }
            else {
                gc_count_line = 999999999; //ありえない値を挿入
            }

            if(gc_count_line == gc_count) { //対象のマークページを検出
                // taiga debug
                std::cout << "gc_count_line " << line << std::endl;
                // taiga debug
                read_flag = true;
                std::getline(file_stream, line);
                if(line != "GC_start") {
                    std::cout << "ERROR:marked pageのファイルが正しくありません" << std::endl;
                    std::cout << "gc_countの次はGC_STARTです" << std::endl;
                    abort();
                }
                // error check
                // full gc check
                if(gc_count != 1) {
                    if(pre_line == "FULL_GC_START") {
                        std::cout << "full gc(ooo_cpu.h)" << std::endl;
                        full_gc_flag = true;
                        break; //full gc なら終了
                    }
                }

                continue;
            }
        }
        if(read_flag) {
            std::string check_start_end = line.substr(0,6);
            if(check_start_end == "sta_ad") {
                // taiga debug
                // std::cout << "sta_ad. gc count " << gc_count << std::endl;
                // taiga debug
                is_there_any_unmarked_pages = true; //unmarked pageがあります
                // error check
                if(start_end_flag != false) {
                    std::cout << "ERROR:unmarked pageのファイルが正しくありません" << std::endl;
                    abort();
                }

                std::string unmarked_start_address_hex = line.substr(9,12);
                try {
                    size_t pos; //デバッグ用
                    v_page_start_address = std::stoull(unmarked_start_address_hex, &pos, 16);

                    // pos が文字列の長さと一致することを確認（余分な文字がないことを確認）
                    if (pos == unmarked_start_address_hex.length()) {
                        // std::cout << "変換成功: " << v_page_start_address << std::endl;
                    } else {
                        std::cerr << "変換失敗: 不正な文字が存在します" << std::endl;
                        exit(1);
                    }
                } catch (const std::invalid_argument& e) {
                    std::cerr << "変換失敗: 無効な引数です" << std::endl;
                    exit(1);
                } catch (const std::out_of_range& e) {
                    std::cerr << "変換失敗: 範囲外です" << std::endl;
                    exit(1);
                }

                start_end_flag = true; //次はend address
            }
            else if(check_start_end == "end_ad") {
                // taiga debug
                // std::cout << "end_ad. gc count " << gc_count << std::endl;
                // taiga debug
                // error check
                if(start_end_flag == false) {
                    std::cout << "ERROR:unmarked pageのファイルが正しくありません" << std::endl;
                    abort();
                }

                // end addressを取得
                std::string unmarked_end_address_hex = line.substr(9,12);
                try {
                    size_t pos; //デバッグ用
                    v_page_end_address = std::stoull(unmarked_end_address_hex, &pos, 16);

                    // pos が文字列の長さと一致することを確認（余分な文字がないことを確認）
                    if (pos == unmarked_end_address_hex.length()) {
                        // std::cout << "変換成功: " << v_page_end_address << std::endl;
                    } else {
                        std::cerr << "変換失敗: 不正な文字が存在します" << std::endl;
                        exit(1);
                    }
                    } catch (const std::invalid_argument& e) {
                    std::cerr << "変換失敗: 無効な引数です" << std::endl;
                    exit(1);
                } catch (const std::out_of_range& e) {
                    std::cerr << "変換失敗: 範囲外です" << std::endl;
                    exit(1);
                }

                // v_unmarked_pagesにマークされたページを追加
                v_page_start_page = v_page_start_address >> LOG2_PAGE_SIZE;
                v_page_end_page = v_page_end_address >> LOG2_PAGE_SIZE;
                // debug
                // std::cout << "v_page_start_page " << v_page_start_page << std::endl;
                // std::cout << "v_page_end_page " << v_page_end_page << std::endl;

                for (uint64_t i = v_page_start_page; i <= v_page_end_page; i++) {
                    v_unmarked_pages.push_back(i);
                }

                start_end_flag = false; //次はstart address
            }
            else if(check_start_end == "GC_end") {
                // taiga debug
                // std::cout << "GC_end. gc count " << gc_count << std::endl;
                // taiga debug
                break;
            }
            else {
                std::cout << "ERROR:unmarked pageのファイルが正しくありません" << std::endl;
                abort();
            }
        }
        pre_line = line;

        // 終端に到達したら折り返し
        if(file_stream.eof()) {
            // エラーチェック
            if(twice_repeat) {
                std::cout << "ERROR:(un)marked pagesの読み込みが行われていません。折り返しが二回目です。" << std::endl;
                std::cout << "gc count " << gc_count << std::endl;
                abort();
            }
            gc_count = 0; //gc_countをリセット
            file_stream.seekg(0, std::ios::beg); //ファイルの先頭に戻る

            twice_repeat = true; // for error check
        }
        
    }
#endif // GC_MARKED_OBJECT

    file_stream.close();

    // error check
    if(start_end_flag == true) {
        std::cout << "ERROR:marked pageのファイルが正しくありません" << std::endl;
        abort();
    }
    // error check
    // 読み込みが行われていない
    if(read_flag == false) {
        std::cout << "ERROR:(un)marked pagesの読み込みが行われていません" << std::endl;
        std::cout << "gc count " << gc_count << std::endl;
        abort();
    }else {
        read_flag = false;
    }

#if (GC_MARKED_OBJECT == ENABLE)
    // マークされたページがなかった場合
    if(is_there_any_marked_pages == false) {
        // debug
        // std::cout << "Warnig : markされたページがありません" << std::endl;
        // debug
    }
#else // GC_MARKED_OBJECT
// マークされたページがなかった場合
    if(is_there_any_unmarked_pages == false) {
        // debug
        // std::cout << "Warnig : unmarkページがありません" << std::endl;
        // debug
    }
#endif // GC_MARKED_OBJECT

#if (GC_MARKED_OBJECT == ENABLE)
    // vpage to ppage
    for (uint64_t i = 0; i < v_marked_pages.size(); i++) {
        uint64_t v_marked_pages_top_address = v_marked_pages.at(i) << LOG2_PAGE_SIZE;
        auto [p_marked_pages_top_address, check_p_address_is_exist] = vmem->va_to_pa(CPU_0, v_marked_pages_top_address); // va_to_paの第一引数変えたほうがいい
        // すでに仮想アドレスに対応する物理アドレスがあれば、check_p_address_is_exist!=0
        if(check_p_address_is_exist != 0) {
            // std::cout << "WARNIG : GC時のマイグレーションにおいて、マークされているオブジェクトの仮想アドレスに対応する物理アドレスがありません" << std::endl;
        }
        uint64_t p_marked_page_address = p_marked_pages_top_address >> LOG2_PAGE_SIZE; //ページアドレスに変換
        p_marked_pages.push_back(p_marked_page_address); 
        // taiga debug
        // std::cout << "p_marked_page " << p_marked_pages.at(i) << std::endl;
        // taiga debug
    }
#else // GC_MARKED_OBJECT
    // vpage to ppage
    vmem->migration_with_gc_start = true;
    for (uint64_t i = 0; i < v_unmarked_pages.size(); i++) {
        uint64_t v_unmarked_pages_top_address = v_unmarked_pages.at(i) << LOG2_PAGE_SIZE;
        
        vmem->migration_with_gc_of_vatopa = true; // 通常の変換ではないことを知らせる
        auto [p_unmarked_pages_top_address, check_p_address_is_exist] = vmem->va_to_pa(CPU_0, v_unmarked_pages_top_address); // va_to_paの第一引数変えたほうがいい
        vmem->migration_with_gc_of_vatopa = false; // 元に戻す

        // すでに仮想アドレスに対応する物理アドレスがあれば、p_unmarked_pages_top_address!=0
        if(p_unmarked_pages_top_address == 0) {
            // std::cout << "WARNIG : GC時のマイグレーションにおいて、マークされているオブジェクトの仮想アドレスに対応する物理アドレスがありません" << std::endl;
        }
        else { // unmarked_pagesがPTEに登録されていた場合（メモリに存在する場合）
            uint64_t p_unmarked_page_address = p_unmarked_pages_top_address >> LOG2_PAGE_SIZE; //ページアドレスに変換
            p_unmarked_pages.push_back(p_unmarked_page_address); 
            // taiga debug
            // std::cout << "p_unmarked_page_address " << p_unmarked_page_address << std::endl;
            // taiga debug
        }
    }

#endif // GC_MARKED_OBJECT
#if (GC_MIGRATION_WITH_GC == ENABLE)
    is_full_gc = full_gc_flag; // full gcかどうかを知らせる
#endif // GC_MIGRATION_WITH_GC
    gc_count = gc_count + 1;
#if (GC_MARKED_OBJECT == ENABLE)
    return p_marked_pages;
#else // GC_MARKED_OBJECT
    return p_unmarked_pages;
#endif // GC_MARKED_OBJECT
}

uint64_t O3_CPU::migration_with_gc(std::vector<std::uint64_t> pages, OS_TRANSPARENT_MANAGEMENT* os_transparent_management) {
    uint64_t count_of_migrations = 0;
    // std::cout << "migration_with_gc here" << std::endl;
    // std::cout << "fast memory capacity " << os_transparent_management->fast_memory_capacity << std::endl;
    // std::cout << "epoch count " << os_transparent_management->epoch_count << std::endl;

    // full gcならスキップ
    if(is_full_gc) {
        is_full_gc = false;
        return count_of_migrations;
    }

    // taiga debug
#if (TEST_HISTORY_WITH_GC == ENABLE)
    // pagesを出力する。GC時に(un)markされているオブジェクトの先頭アドレスの中でva_to_paに登録されているページを確認
    if(pages.size() == 0) {
        std::cout << "(un)markされているオブジェクトでvpage_to_ppageに登録されているものはありません。" << std::endl;
    }
    else {
        std::cout << "(un)markされているオブジェクトの先頭アドレスで、vpage_to_ppageに登録されているもの" << std::endl;
        for(uint64_t i = 0; i < pages.size(); i++) {
            std::cout << pages.at(i) << std::endl;
        }
    }
    
#endif // TEST_HISTORY_WITH_GC
    // taiga debug

    // リマッピングリクエスト作成
#if (GC_MARKED_OBJECT == ENABLE)
    os_transparent_management->choose_hotpage_with_sort_with_gc(pages);
#else // GC_MARKED_OBJECT
    os_transparent_management->choose_hotpage_with_sort_with_gc_unmarked(pages);
#endif // GC_MARKED_OBJECT
    os_transparent_management->add_new_remapping_request_to_queue_with_gc(pages);
    count_of_migrations = os_transparent_management->migration_all_start_with_gc();
    os_transparent_management->initialize_hotness_table_with_gc(os_transparent_management->hotness_table_with_gc);

    return count_of_migrations;

#endif // GC_MIGRATION_WITH_GC
// taiga added
}

void O3_CPU::do_memory_scheduling(ooo_model_instr& instr)
{
    // load
    for (auto& smem : instr.source_memory)
    {
        auto q_entry = std::find_if_not(std::begin(LQ), std::end(LQ), [](const auto& lq_entry)
            { return lq_entry.has_value(); });
        assert(q_entry != std::end(LQ));
        q_entry->emplace(instr.instr_id, smem, instr.ip, instr.asid); // add it to the load queue

        // Check for forwarding
        auto sq_it = std::max_element(std::begin(SQ), std::end(SQ), [smem](const auto& lhs, const auto& rhs)
            { return lhs.virtual_address != smem || (rhs.virtual_address == smem && lhs.instr_id < rhs.instr_id); });
        if (sq_it != std::end(SQ) && sq_it->virtual_address == smem)
        {
            if (sq_it->fetch_issued)
            { // Store already executed
                q_entry->reset();
                ++instr.completed_mem_ops;

                if constexpr (champsim::debug_print)
                {
#if (USE_VCPKG == ENABLE)
                    fmt::print("[DISPATCH] {} instr_id: {} forwards_from: {}\n", __func__, instr.instr_id, sq_it->event_cycle);
#endif // USE_VCPKG
                }
            }
            else
            {
                assert(sq_it->instr_id < instr.instr_id);   // The found SQ entry is a prior store
                sq_it->lq_depend_on_me.push_back(*q_entry); // Forward the load when the store finishes
                (*q_entry)->producer_id = sq_it->instr_id;  // The load waits on the store to finish

                if constexpr (champsim::debug_print)
                {
#if (USE_VCPKG == ENABLE)
                    fmt::print("[DISPATCH] {} instr_id: {} waits on: {}\n", __func__, instr.instr_id, sq_it->event_cycle);
#endif // USE_VCPKG
                }
            }
        }
    }

    // store
    for (auto& dmem : instr.destination_memory)
        SQ.emplace_back(instr.instr_id, dmem, instr.ip, instr.asid); // add it to the store queue

    if constexpr (champsim::debug_print)
    {
#if (USE_VCPKG == ENABLE)
        fmt::print("[DISPATCH] {} instr_id: {} loads: {} stores: {}\n", __func__, instr.instr_id, std::size(instr.source_memory),
            std::size(instr.destination_memory));
#endif // USE_VCPKG
    }
}

long O3_CPU::operate_lsq()
{
    auto store_bw          = SQ_WIDTH;

    const auto complete_id = std::empty(ROB) ? std::numeric_limits<uint64_t>::max() : ROB.front().instr_id;
    auto do_complete       = [cycle = current_cycle, complete_id, this](const auto& x)
    {
        return x.instr_id < complete_id && x.event_cycle <= cycle && this->do_complete_store(x);
    };

    auto unfetched_begin          = std::partition_point(std::begin(SQ), std::end(SQ), [](const auto& x)
                 { return x.fetch_issued; });
    auto [fetch_begin, fetch_end] = champsim::get_span_p(unfetched_begin, std::end(SQ), store_bw,
        [cycle = current_cycle](const auto& x)
        { return ! x.fetch_issued && x.event_cycle <= cycle; });
    store_bw -= std::distance(fetch_begin, fetch_end);
    std::for_each(fetch_begin, fetch_end, [cycle = current_cycle, this](auto& sq_entry)
        {
    this->do_finish_store(sq_entry);
    sq_entry.fetch_issued = true;
    sq_entry.event_cycle = cycle; });

    auto [complete_begin, complete_end] = champsim::get_span_p(std::cbegin(SQ), std::cend(SQ), store_bw, do_complete);
    store_bw -= std::distance(complete_begin, complete_end);
    SQ.erase(complete_begin, complete_end);

    auto load_bw = LQ_WIDTH;

    for (auto& lq_entry : LQ)
    {
        if (load_bw > 0 && lq_entry.has_value() && lq_entry->producer_id == std::numeric_limits<uint64_t>::max() && ! lq_entry->fetch_issued && lq_entry->event_cycle < current_cycle)
        {
            auto success = execute_load(*lq_entry);
            if (success)
            {
                --load_bw;
                lq_entry->fetch_issued = true;
            }
        }
    }

    return (SQ_WIDTH - store_bw) + (LQ_WIDTH - load_bw);
}

void O3_CPU::do_finish_store(const LSQ_ENTRY& sq_entry)
{
    sq_entry.finish(std::begin(ROB), std::end(ROB));

    // Release dependent loads
    for (std::optional<LSQ_ENTRY>& dependent : sq_entry.lq_depend_on_me)
    {
        assert(dependent.has_value()); // LQ entry is still allocated
        assert(dependent->producer_id == sq_entry.instr_id);

        dependent->finish(std::begin(ROB), std::end(ROB));
        dependent.reset();
    }
}

bool O3_CPU::do_complete_store(const LSQ_ENTRY& sq_entry)
{
    CacheBus::request_type data_packet;
    data_packet.v_address = sq_entry.virtual_address;
    data_packet.instr_id  = sq_entry.instr_id;
    data_packet.ip        = sq_entry.ip;

    if constexpr (champsim::debug_print)
    {
#if (USE_VCPKG == ENABLE)
        fmt::print("[SQ] {} instr_id: {} vaddr: {:x}\n", __func__, data_packet.instr_id, data_packet.v_address);
#endif // USE_VCPKG
    }

// #if (PRINT_V_ADDRESS == ENABLE) // debug
//     std::ofstream output_file_pva("/home/funkytaiga/tmp_champ/ChampSim-Ramulator/tmp_print_v_address.txt", std::ios::app);
//     if(!output_file_pva.is_open()) {
//         std::cerr << "/home/funkytaiga/tmp_champ/ChampSim-Ramulator/tmp_print_v_address.txtが開けませんでした。" << std::endl;
//         abort();
//     }
//     output_file_pva << "0x" << std::hex << data_packet.v_address << std::dec << " (write)" << std::endl;
//     output_file_pva.close();
// #endif //debug

    return L1D_bus.issue_write(data_packet);
}

bool O3_CPU::execute_load(const LSQ_ENTRY& lq_entry)
{
    CacheBus::request_type data_packet;
    data_packet.v_address = lq_entry.virtual_address;
    data_packet.instr_id  = lq_entry.instr_id;
    data_packet.ip        = lq_entry.ip;

    if constexpr (champsim::debug_print)
    {
#if (USE_VCPKG == ENABLE)
        fmt::print("[LQ] {} instr_id: {} vaddr: {:#x}\n", __func__, data_packet.instr_id, data_packet.v_address);
#endif // USE_VCPKG
    }

// #if (PRINT_V_ADDRESS == ENABLE) // debug
//     std::ofstream output_file_pva("/home/funkytaiga/tmp_champ/ChampSim-Ramulator/tmp_print_v_address.txt", std::ios::app);
//         if(!output_file_pva.is_open()) {
//             std::cerr << "/home/funkytaiga/tmp_champ/ChampSim-Ramulator/tmp_print_v_address.txtが開けませんでした。" << std::endl;
//             abort();
//         }
//         output_file_pva << "0x" << std::hex << data_packet.v_address << std::dec << " (load)" << std::endl;
//         output_file_pva.close();
// #endif //debug

    return L1D_bus.issue_read(data_packet);
}

void O3_CPU::do_complete_execution(ooo_model_instr& instr)
{
    for (auto dreg : instr.destination_registers)
    {
        auto begin = std::begin(reg_producers[dreg]);
        auto end   = std::end(reg_producers[dreg]);
        auto elem  = std::find_if(begin, end, [id = instr.instr_id](ooo_model_instr& x)
             { return x.instr_id == id; });
        assert(elem != end);
        reg_producers[dreg].erase(elem);
    }

    instr.executed = COMPLETED;

    for (ooo_model_instr& dependent : instr.registers_instrs_depend_on_me)
    {
        dependent.num_reg_dependent--;
        assert(dependent.num_reg_dependent >= 0);

        if (dependent.num_reg_dependent == 0)
            dependent.scheduled = COMPLETED;
    }

    if (instr.branch_mispredicted)
        fetch_resume_cycle = current_cycle + BRANCH_MISPREDICT_PENALTY;
}

long O3_CPU::complete_inflight_instruction()
{
    // update ROB entries with completed executions
    auto complete_bw = EXEC_WIDTH;
    for (auto rob_it = std::begin(ROB); rob_it != std::end(ROB) && complete_bw > 0; ++rob_it)
    {
        if ((rob_it->executed == INFLIGHT) && (rob_it->event_cycle <= current_cycle) && rob_it->completed_mem_ops == rob_it->num_mem_ops())
        {
            do_complete_execution(*rob_it);
            --complete_bw;
        }
    }

    return EXEC_WIDTH - complete_bw;
}

long O3_CPU::handle_memory_return()
{
    long progress {0};

    for (auto l1i_bw = FETCH_WIDTH, to_read = L1I_BANDWIDTH; l1i_bw > 0 && to_read > 0 && ! L1I_bus.lower_level->returned.empty(); --to_read)
    {
        auto& l1i_entry = L1I_bus.lower_level->returned.front();

        while (l1i_bw > 0 && ! l1i_entry.instr_depend_on_me.empty())
        {
            ooo_model_instr& fetched = l1i_entry.instr_depend_on_me.front();
            if ((fetched.ip >> LOG2_BLOCK_SIZE) == (l1i_entry.v_address >> LOG2_BLOCK_SIZE) && fetched.fetched != 0)
            {
                fetched.fetched = COMPLETED;
                --l1i_bw;
                ++progress;

                if constexpr (champsim::debug_print)
                {
#if (USE_VCPKG == ENABLE)
                    fmt::print("[IFETCH] {} instr_id: {} fetch completed\n", __func__, fetched.instr_id);
#endif // USE_VCPKG
                }
            }

            l1i_entry.instr_depend_on_me.erase(std::begin(l1i_entry.instr_depend_on_me));
        }

        // remove this entry if we have serviced all of its instructions
        if (l1i_entry.instr_depend_on_me.empty())
        {
            L1I_bus.lower_level->returned.pop_front();
            ++progress;
        }
    }

    auto l1d_it = std::begin(L1D_bus.lower_level->returned);
    for (auto l1d_bw = L1D_BANDWIDTH; l1d_bw > 0 && l1d_it != std::end(L1D_bus.lower_level->returned); --l1d_bw, ++l1d_it)
    {
        for (auto& lq_entry : LQ)
        {
            if (lq_entry.has_value() && lq_entry->fetch_issued && lq_entry->virtual_address >> LOG2_BLOCK_SIZE == l1d_it->v_address >> LOG2_BLOCK_SIZE)
            {
                lq_entry->finish(std::begin(ROB), std::end(ROB));
                lq_entry.reset();
                ++progress;
            }
        }
        ++progress;
    }
    L1D_bus.lower_level->returned.erase(std::begin(L1D_bus.lower_level->returned), l1d_it);

    return progress;
}

long O3_CPU::retire_rob()
{
    auto [retire_begin, retire_end] = champsim::get_span_p(std::cbegin(ROB), std::cend(ROB), RETIRE_WIDTH, [](const auto& x)
        { return x.executed == COMPLETED; });
    if constexpr (champsim::debug_print)
    {
#if (USE_VCPKG == ENABLE)
        std::for_each(retire_begin, retire_end, [](const auto& x)
            { fmt::print("[ROB] retire_rob instr_id: {} is retired\n", x.instr_id); });
#endif // USE_VCPKG
    }
    auto retire_count = std::distance(retire_begin, retire_end);
    num_retired += retire_count;
    ROB.erase(retire_begin, retire_end);
#if(HISTORY_BASED_PAGE_SELECTION == ENABLE)
    // taiga added
    // migration?
#if(NO_MIGRATION == DISABLE)
    static uint64_t pre_instr = 0;
    if(num_retired - pre_instr > EPOCH_LENGTH && num_retired - os_transparent_management->pre_migration_instr > DISTANCE_MIGRATION) {
        // taiga debug
        std::cout << "num_retired " << num_retired << std::endl;
        // taiga debug
        os_transparent_management->choose_hotpage_with_sort();
        os_transparent_management->add_new_remapping_request_to_queue(0); // remapping_queueを無視（いつか直す）
        pre_instr = num_retired;
        os_transparent_management->clear_counter_table_epoch_count++;
        os_transparent_management->initialize_hotness_table(os_transparent_management->hotness_table);
        // カウンターテーブルの初期化
        // CLEAR_COUNTER_TABLE_EPOCH_NUMエポック毎に行う
        if(os_transparent_management->clear_counter_table_epoch_count >= CLEAR_COUNTER_TABLE_EPOCH_NUM) {
            os_transparent_management->initialize_counter_table(os_transparent_management->counter_table);
            os_transparent_management->clear_counter_table_epoch_count = 0;
        }

        os_transparent_management->pre_migration_instr = num_retired;
    }
#endif // NO_MIGRATION
    // taiga added
#endif // HISTORY_BASED_PAGE_SELECTION
    return retire_count;
}

// LCOV_EXCL_START Exclude the following function from LCOV
void O3_CPU::print_deadlock()
{
#if (USE_VCPKG == ENABLE)
    fmt::print("DEADLOCK! CPU {} cycle {}\n", cpu, current_cycle);

    auto instr_pack = [](const auto& entry)
    {
        return std::tuple {entry.instr_id, +entry.fetched, +entry.scheduled, +entry.executed, +entry.num_reg_dependent, entry.num_mem_ops() - entry.completed_mem_ops, entry.event_cycle};
    };
    std::string_view instr_fmt {"instr_id: {} fetched: {} scheduled: {} executed: {} num_reg_dependent: {} num_mem_ops: {} event: {}"};

    champsim::range_print_deadlock(IFETCH_BUFFER, "cpu" + std::to_string(cpu) + "_IFETCH", instr_fmt, instr_pack);
    champsim::range_print_deadlock(DECODE_BUFFER, "cpu" + std::to_string(cpu) + "_DECODE", instr_fmt, instr_pack);
    champsim::range_print_deadlock(DISPATCH_BUFFER, "cpu" + std::to_string(cpu) + "_DISPATCH", instr_fmt, instr_pack);
    champsim::range_print_deadlock(ROB, "cpu" + std::to_string(cpu) + "_ROB", instr_fmt, instr_pack);

    // print LSQ entries
    auto lq_pack = [](const auto& entry)
    {
        std::string depend_id {"-"};
        if (entry->producer_id != std::numeric_limits<uint64_t>::max())
        {
            depend_id = std::to_string(entry->producer_id);
        }
        return std::tuple {entry->instr_id, entry->virtual_address, entry->fetch_issued, entry->event_cycle, depend_id};
    };
    std::string_view lq_fmt {"instr_id: {} address: {:#x} fetch_issued: {} event_cycle: {} waits on {}"};

    auto sq_pack = [](const auto& entry)
    {
        std::vector<uint64_t> depend_ids;
        std::transform(std::begin(entry.lq_depend_on_me), std::end(entry.lq_depend_on_me), std::back_inserter(depend_ids),
            [](const std::optional<LSQ_ENTRY>& lq_entry)
            { return lq_entry->producer_id; });
        return std::tuple {entry.instr_id, entry.virtual_address, entry.fetch_issued, entry.event_cycle, depend_ids};
    };
    std::string_view sq_fmt {"instr_id: {} address: {:#x} fetch_issued: {} event_cycle: {} LQ waiting: {}"};
    champsim::range_print_deadlock(LQ, "cpu" + std::to_string(cpu) + "_LQ", lq_fmt, lq_pack);
    champsim::range_print_deadlock(SQ, "cpu" + std::to_string(cpu) + "_SQ", sq_fmt, sq_pack);
#endif // USE_VCPKG
}

// LCOV_EXCL_STOP

LSQ_ENTRY::LSQ_ENTRY(uint64_t id, uint64_t addr, uint64_t local_ip, std::array<uint8_t, 2> local_asid)
: instr_id(id), virtual_address(addr), ip(local_ip), asid(local_asid)
{
}

void LSQ_ENTRY::finish(std::deque<ooo_model_instr>::iterator begin, std::deque<ooo_model_instr>::iterator end) const
{
    auto rob_entry = std::partition_point(begin, end, [id = this->instr_id](auto x)
        { return x.instr_id < id; });
    assert(rob_entry != end);
    assert(rob_entry->instr_id == this->instr_id);

    ++rob_entry->completed_mem_ops;
    assert(rob_entry->completed_mem_ops <= rob_entry->num_mem_ops());

    if constexpr (champsim::debug_print)
    {
#if (USE_VCPKG == ENABLE)
        fmt::print("[LSQ] {} instr_id: {} full_address: {:#x} remain_mem_ops: {} event_cycle: {}\n", __func__, instr_id, virtual_address,
            rob_entry->num_mem_ops() - rob_entry->completed_mem_ops, event_cycle);
#endif // USE_VCPKG
    }
}

bool CacheBus::issue_read(request_type data_packet)
{
    data_packet.address       = data_packet.v_address;
    data_packet.is_translated = false;
    data_packet.cpu           = cpu;
    data_packet.type          = access_type::LOAD;

#if (TRACKING_LOAD_STORE_STATISTICS == ENABLE)
    data_packet.type_origin = access_type::LOAD;
#endif // TRACKING_LOAD_STORE_STATISTICS

    return lower_level->add_rq(data_packet);
}

bool CacheBus::issue_write(request_type data_packet)
{
    data_packet.address            = data_packet.v_address;
    data_packet.is_translated      = false;
    data_packet.cpu                = cpu;
    data_packet.type               = access_type::WRITE;
    data_packet.response_requested = false;

#if (TRACKING_LOAD_STORE_STATISTICS == ENABLE)
    data_packet.type_origin = access_type::WRITE;
#endif // TRACKING_LOAD_STORE_STATISTICS

    return lower_level->add_wq(data_packet);
}

#if (PRINT_V_ADDRESS == ENABLE) // debug
    void O3_CPU::print_lq_v_address(ooo_model_instr& instr){
        std::ofstream output_file_pva("/home/funkytaiga/tmp_champ/ChampSim-Ramulator/tmp_print_v_address.txt", std::ios::app);
        if(!output_file_pva.is_open()) {
            std::cerr << "/home/funkytaiga/tmp_champ/ChampSim-Ramulator/tmp_print_v_address.txtが開けませんでした。" << std::endl;
            abort();
        }

        for(int i = 0; i < instr.source_memory.size(); i++) {
            output_file_pva << instr.source_memory.at(i) << " source memory(load)" << std::endl;
            if(i != 0) {
                output_file_pva << "souce memory has two elements(load)" << std::endl;
            }
        }
        for(int i = 0; i < instr.destination_memory.size(); i++) {
            output_file_pva << instr.destination_memory.at(i) << " destination_memory(load)" << std::endl;
            if(i != 0) {
                output_file_pva << "destination_memory has two elements(load)" << std::endl;
            }
        }

        output_file_pva.close();
    }
    void O3_CPU::print_sq_v_address(ooo_model_instr& instr){
        std::ofstream output_file_pva("/home/funkytaiga/tmp_champ/ChampSim-Ramulator/tmp_print_v_address.txt", std::ios::app);
        if(!output_file_pva.is_open()) {
            std::cerr << "/home/funkytaiga/tmp_champ/ChampSim-Ramulator/tmp_print_v_address.txtが開けませんでした。" << std::endl;
            abort();
        }

        for(long unsigned int i = 0; i < instr.source_memory.size(); i++) {
            output_file_pva << instr.source_memory.at(i) << " source memory(store)" << std::endl;
            if(i != 0) {
                output_file_pva << "souce memory has two elements(store)" << std::endl;
            }
        }
        for(int i = 0; i < instr.destination_memory.size(); i++) {
            output_file_pva << instr.destination_memory.at(i) << " destination_memory(store)" << std::endl;
            if(i != 0) {
                output_file_pva << "destination_memory has two elements(store)" << std::endl;
            }
        }

        output_file_pva.close();
    }
#endif // PRINT_V_ADDRESS