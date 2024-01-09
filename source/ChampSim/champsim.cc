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

#include "ChampSim/champsim.h"

#include "ProjectConfiguration.h" // User file

#if (USE_VCPKG == ENABLE)
#include <fmt/chrono.h>
#include <fmt/core.h>
#endif // USE_VCPKG

#include <algorithm>
#include <chrono>
#include <numeric>

#include "ChampSim/ooo_cpu.h"
#include "ChampSim/operable.h"

constexpr int DEADLOCK_CYCLE {500};

auto start_time = std::chrono::steady_clock::now();

std::chrono::seconds elapsed_time() { return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start_time); }

#if (USER_CODES == ENABLE)

#if (RAMULATOR == ENABLE)
namespace champsim
{
phase_stats do_phase(phase_info phase, environment& env, std::vector<tracereader>& traces)
{
    auto [phase_name, is_warmup, length, trace_index, trace_names] = phase;
    auto operables                                                 = env.operable_view();

    // Initialize phase
    for (champsim::operable& op : operables)
    {
        op.warmup = is_warmup;
        op.begin_phase();
    }

    // Perform phase
    int stalled_cycle {0};
    std::vector<bool> phase_complete(std::size(env.cpu_view()), false);
    while (! std::accumulate(std::begin(phase_complete), std::end(phase_complete), true, std::logical_and {}))
    {
        auto next_phase_complete = phase_complete;

        // Operate
        long progress {0};
        for (champsim::operable& op : operables)
        {
            progress += op._operate();
        }

        if (progress == 0)
        {
            ++stalled_cycle;
        }
        else
        {
            stalled_cycle = 0;
        }

        if (stalled_cycle >= DEADLOCK_CYCLE)
        {
            std::for_each(std::begin(operables), std::end(operables), [](champsim::operable& c)
                { c.print_deadlock(); });
            abort();
        }

        std::sort(std::begin(operables), std::end(operables),
            [](const champsim::operable& lhs, const champsim::operable& rhs)
            { return lhs.leap_operation < rhs.leap_operation; });

        // Read from trace
        for (O3_CPU& cpu : env.cpu_view())
        {
            auto& trace = traces.at(trace_index.at(cpu.cpu));
            for (auto pkt_count = cpu.IN_QUEUE_SIZE - static_cast<long>(std::size(cpu.input_queue)); ! trace.eof() && pkt_count > 0; --pkt_count) {
#if (GC_MIGRATION_WITH_GC==ENABLE) 
#if (PRINT_V_ADDRESS_TRACE == ENABLE)
//              // taiga debug
                auto instr = trace();
                std::ofstream output_file_pva_trace("/home/funkytaiga/tmp_champ/ChampSim-Ramulator/tmp_print_v_address_trace.txt", std::ios::app);
                if(!output_file_pva_trace.is_open()) {
                    std::cerr << "/home/funkytaiga/tmp_champ/ChampSim-Ramulator/tmp_print_v_address_trace.txtが開けませんでした。" << std::endl;
                    abort();
                }

                for(long unsigned int i = 0; i < instr.source_memory.size(); i++) {
                    output_file_pva_trace << instr.source_memory.at(i) << " source memory(store)" << std::endl;
                    if(i != 0) {
                        output_file_pva_trace << "souce memory has two elements(store)" << std::endl;
                    }
                }
                for(long unsigned int i = 0; i < instr.destination_memory.size(); i++) {
                    output_file_pva_trace << instr.destination_memory.at(i) << " destination_memory(store)" << std::endl;
                    if(i != 0) {
                        output_file_pva_trace << "destination_memory has two elements(store)" << std::endl;
                    }
                }

                output_file_pva_trace.close();
                // if(trace().is_branch == 1) {
                //     std::cout << "===================================" << std::endl;
                //     std::cout << "champsim.cc : is_branch == 1" << std::endl;
                // }
                // static long trace_count_for_debug = 0;
                // std::cout << "trace_count : " << trace_count_for_debug << std::endl;
                // auto tmp_trace = trace();
                // trace_count_for_debug++;
                // std::cout << "============GC_TRACE===========" << std::endl;
                // std::cout << "======================== " << tmp_trace.ip << " ======================== " << std::endl;
                // if(tmp_trace.is_gc_rtn_start != 0) {
                //     std::cout << "===================================" << std::endl;
                //     std::cout << "champsim.cc : is_gc_rtn_start = " << tmp_trace.is_gc_rtn_start << std::endl;
                // }
                // if(tmp_trace.is_mark_end == 1) {
                //     std::cout << "champsim.cc : is_mark_end == 1" << std::endl;
                // }
                // static int print_count_limit = 0;
                // if(print_count_limit < 100) {
                //     std::cout << "==============" << std::endl;
                //     std::cout << "tmp_trace.ip " << tmp_trace.ip << std::endl;
                //     std::cout << "tmp_trace.branch_taken " << tmp_trace.branch_taken << std::endl;
                //     // std::ostream_iteratorを使用してstd::vectorを標準出力に出力
                //     std::cout << "tmp_trace.source_registers ";
                //     std::copy(tmp_trace.source_registers.begin(), tmp_trace.source_registers.end(), std::ostream_iterator<unsigned char>(std::cout, " "));
                //     std::cout << std::endl;
                //     std::cout << "tmp_trace.destination_memory ";
                //     std::copy(tmp_trace.destination_memory.begin(), tmp_trace.destination_memory.end(), std::ostream_iterator<unsigned char>(std::cout, " "));
                //     std::cout << std::endl;
                //     std::cout << "tmp_trace.destination_registers ";
                //     std::copy(tmp_trace.destination_registers.begin(), tmp_trace.destination_registers.end(), std::ostream_iterator<unsigned char>(std::cout, " "));
                //     std::cout << std::endl;
                //     std::cout << "tmp_trace.source_memory ";
                //     std::copy(tmp_trace.source_memory.begin(), tmp_trace.source_memory.end(), std::ostream_iterator<unsigned char>(std::cout, " "));
                //     std::cout << std::endl;
                //     std::cout << "tmp_trace.source_memory ";
                //     std::copy(tmp_trace.source_memory.begin(), tmp_trace.source_memory.end(), std::ostream_iterator<unsigned char>(std::cout, " "));
                //     std::cout << std::endl;
                //     std::cout << "tmp_trace.is_gc_rtn_start " << tmp_trace.is_gc_rtn_start << std::endl;
                //     std::cout << "tmp_trace.is_gc_rtn_end " << tmp_trace.is_gc_rtn_end << std::endl;
                //     std::cout << "tmp_trace.is_mark_end " << tmp_trace.is_mark_end << std::endl;
                //     std::cout << "==============" << std::endl;
                // }
//              // taiga debug
#endif // PRINT_V_ADDRESS_TRACE
#endif // GC_MIGRATION_WITH_GC
                // cpu.input_queue.push_back(tmp_trace);
                cpu.input_queue.push_back(trace());
            }
                

            // If any trace reaches EOF, terminate all phases
            if (trace.eof())
                std::fill(std::begin(next_phase_complete), std::end(next_phase_complete), true);
        }

        // Check for phase finish
        for (O3_CPU& cpu : env.cpu_view())
        {
            // Phase complete
            next_phase_complete[cpu.cpu] = next_phase_complete[cpu.cpu] || (cpu.sim_instr() >= length);
        }

        for (O3_CPU& cpu : env.cpu_view())
        {
            if (next_phase_complete[cpu.cpu] != phase_complete[cpu.cpu])
            {
                for (champsim::operable& op : operables)
                    op.end_phase(cpu.cpu);

#if (USE_VCPKG == ENABLE)
                fmt::print("{} finished CPU {} instructions: {} cycles: {} cumulative IPC: {:.4g} (Simulation time: {:%H hr %M min %S sec})\n", phase_name, cpu.cpu,
                    cpu.sim_instr(), cpu.sim_cycle(), std::ceil(cpu.sim_instr()) / std::ceil(cpu.sim_cycle()), elapsed_time());
#endif // USE_VCPKG
            }
        }

        phase_complete = next_phase_complete;
    }

#if (USE_VCPKG == ENABLE)
    for (O3_CPU& cpu : env.cpu_view())
    {
        fmt::print("{} complete CPU {} instructions: {} cycles: {} cumulative IPC: {:.4g} (Simulation time: {:%H hr %M min %S sec})\n", phase_name, cpu.cpu,
            cpu.sim_instr(), cpu.sim_cycle(), std::ceil(cpu.sim_instr()) / std::ceil(cpu.sim_cycle()), elapsed_time());
    }
#endif // USE_VCPKG

    phase_stats stats;
    stats.name = phase.name;

    for (std::size_t i = 0; i < std::size(trace_index); ++i)
        stats.trace_names.push_back(trace_names.at(trace_index.at(i)));

    auto cpus = env.cpu_view();
    std::transform(std::begin(cpus), std::end(cpus), std::back_inserter(stats.sim_cpu_stats), [](const O3_CPU& cpu)
        { return cpu.sim_stats; });
    std::transform(std::begin(cpus), std::end(cpus), std::back_inserter(stats.roi_cpu_stats), [](const O3_CPU& cpu)
        { return cpu.roi_stats; });

    auto caches = env.cache_view();
    std::transform(std::begin(caches), std::end(caches), std::back_inserter(stats.sim_cache_stats), [](const CACHE& cache)
        { return cache.sim_stats; });
    std::transform(std::begin(caches), std::end(caches), std::back_inserter(stats.roi_cache_stats), [](const CACHE& cache)
        { return cache.roi_stats; });

    return stats;
}

// simulation entry point
std::vector<phase_stats> main(environment& env, std::vector<phase_info>& phases, std::vector<tracereader>& traces)
{
    for (champsim::operable& op : env.operable_view())
        op.initialize();

    std::vector<phase_stats> results;
    for (auto phase : phases)
    {
        auto stats = do_phase(phase, env, traces);
        if (! phase.is_warmup)
            results.push_back(stats);
    }

    return results;
}
} // namespace champsim

#else

namespace champsim
{
phase_stats do_phase(phase_info phase, environment& env, std::vector<tracereader>& traces)
{
    auto [phase_name, is_warmup, length, trace_index, trace_names] = phase;
    auto operables                                                 = env.operable_view();

    // Initialize phase
    for (champsim::operable& op : operables)
    {
        op.warmup = is_warmup;
        op.begin_phase();
    }

    // Perform phase
    int stalled_cycle {0};
    std::vector<bool> phase_complete(std::size(env.cpu_view()), false);
    while (! std::accumulate(std::begin(phase_complete), std::end(phase_complete), true, std::logical_and {}))
    {
        auto next_phase_complete = phase_complete;

        // Operate
        long progress {0};
        for (champsim::operable& op : operables)
        {
            progress += op._operate();
        }

        if (progress == 0)
        {
            ++stalled_cycle;
        }
        else
        {
            stalled_cycle = 0;
        }

        if (stalled_cycle >= DEADLOCK_CYCLE)
        {
            std::for_each(std::begin(operables), std::end(operables), [](champsim::operable& c)
                { c.print_deadlock(); });
            abort();
        }

        std::sort(std::begin(operables), std::end(operables),
            [](const champsim::operable& lhs, const champsim::operable& rhs)
            { return lhs.leap_operation < rhs.leap_operation; });

        // Read from trace
        for (O3_CPU& cpu : env.cpu_view())
        {
            auto& trace = traces.at(trace_index.at(cpu.cpu));
            for (auto pkt_count = cpu.IN_QUEUE_SIZE - static_cast<long>(std::size(cpu.input_queue)); ! trace.eof() && pkt_count > 0; --pkt_count)
                cpu.input_queue.push_back(trace());

            // If any trace reaches EOF, terminate all phases
            if (trace.eof())
                std::fill(std::begin(next_phase_complete), std::end(next_phase_complete), true);
        }

        // Check for phase finish
        for (O3_CPU& cpu : env.cpu_view())
        {
            // Phase complete
            next_phase_complete[cpu.cpu] = next_phase_complete[cpu.cpu] || (cpu.sim_instr() >= length);
        }

        for (O3_CPU& cpu : env.cpu_view())
        {
            if (next_phase_complete[cpu.cpu] != phase_complete[cpu.cpu])
            {
                for (champsim::operable& op : operables)
                    op.end_phase(cpu.cpu);

#if (USE_VCPKG == ENABLE)
                fmt::print("{} finished CPU {} instructions: {} cycles: {} cumulative IPC: {:.4g} (Simulation time: {:%H hr %M min %S sec})\n", phase_name, cpu.cpu,
                    cpu.sim_instr(), cpu.sim_cycle(), std::ceil(cpu.sim_instr()) / std::ceil(cpu.sim_cycle()), elapsed_time());
#endif // USE_VCPKG
            }
        }

        phase_complete = next_phase_complete;
    }

#if (USE_VCPKG == ENABLE)
    for (O3_CPU& cpu : env.cpu_view())
    {
        fmt::print("{} complete CPU {} instructions: {} cycles: {} cumulative IPC: {:.4g} (Simulation time: {:%H hr %M min %S sec})\n", phase_name, cpu.cpu,
            cpu.sim_instr(), cpu.sim_cycle(), std::ceil(cpu.sim_instr()) / std::ceil(cpu.sim_cycle()), elapsed_time());
    }
#endif // USE_VCPKG

    phase_stats stats;
    stats.name = phase.name;

    for (std::size_t i = 0; i < std::size(trace_index); ++i)
        stats.trace_names.push_back(trace_names.at(trace_index.at(i)));

    auto cpus = env.cpu_view();
    std::transform(std::begin(cpus), std::end(cpus), std::back_inserter(stats.sim_cpu_stats), [](const O3_CPU& cpu)
        { return cpu.sim_stats; });
    std::transform(std::begin(cpus), std::end(cpus), std::back_inserter(stats.roi_cpu_stats), [](const O3_CPU& cpu)
        { return cpu.roi_stats; });

    auto caches = env.cache_view();
    std::transform(std::begin(caches), std::end(caches), std::back_inserter(stats.sim_cache_stats), [](const CACHE& cache)
        { return cache.sim_stats; });
    std::transform(std::begin(caches), std::end(caches), std::back_inserter(stats.roi_cache_stats), [](const CACHE& cache)
        { return cache.roi_stats; });

    auto dram = env.dram_view();
    std::transform(std::begin(dram.channels), std::end(dram.channels), std::back_inserter(stats.sim_dram_stats),
        [](const DRAM_CHANNEL& chan)
        { return chan.sim_stats; });
    std::transform(std::begin(dram.channels), std::end(dram.channels), std::back_inserter(stats.roi_dram_stats),
        [](const DRAM_CHANNEL& chan)
        { return chan.roi_stats; });

    return stats;
}

// simulation entry point
std::vector<phase_stats> main(environment& env, std::vector<phase_info>& phases, std::vector<tracereader>& traces)
{
    for (champsim::operable& op : env.operable_view())
        op.initialize();

    std::vector<phase_stats> results;
    for (auto phase : phases)
    {
        auto stats = do_phase(phase, env, traces);
        if (! phase.is_warmup)
            results.push_back(stats);
    }

    return results;
}
} // namespace champsim

#endif // RAMULATOR

#else
/* Original code of ChampSim */

namespace champsim
{
phase_stats do_phase(phase_info phase, environment& env, std::vector<tracereader>& traces)
{
    auto [phase_name, is_warmup, length, trace_index, trace_names] = phase;
    auto operables                                                 = env.operable_view();

    // Initialize phase
    for (champsim::operable& op : operables)
    {
        op.warmup = is_warmup;
        op.begin_phase();
    }

    // Perform phase
    int stalled_cycle {0};
    std::vector<bool> phase_complete(std::size(env.cpu_view()), false);
    while (! std::accumulate(std::begin(phase_complete), std::end(phase_complete), true, std::logical_and {}))
    {
        auto next_phase_complete = phase_complete;

        // Operate
        long progress {0};
        for (champsim::operable& op : operables)
        {
            progress += op._operate();
        }

        if (progress == 0)
        {
            ++stalled_cycle;
        }
        else
        {
            stalled_cycle = 0;
        }

        if (stalled_cycle >= DEADLOCK_CYCLE)
        {
            std::for_each(std::begin(operables), std::end(operables), [](champsim::operable& c)
                { c.print_deadlock(); });
            abort();
        }

        std::sort(std::begin(operables), std::end(operables),
            [](const champsim::operable& lhs, const champsim::operable& rhs)
            { return lhs.leap_operation < rhs.leap_operation; });

        // Read from trace
        for (O3_CPU& cpu : env.cpu_view())
        {
            auto& trace = traces.at(trace_index.at(cpu.cpu));
            for (auto pkt_count = cpu.IN_QUEUE_SIZE - static_cast<long>(std::size(cpu.input_queue)); ! trace.eof() && pkt_count > 0; --pkt_count)
                cpu.input_queue.push_back(trace());

            // If any trace reaches EOF, terminate all phases
            if (trace.eof())
                std::fill(std::begin(next_phase_complete), std::end(next_phase_complete), true);
        }

        // Check for phase finish
        for (O3_CPU& cpu : env.cpu_view())
        {
            // Phase complete
            next_phase_complete[cpu.cpu] = next_phase_complete[cpu.cpu] || (cpu.sim_instr() >= length);
        }

        for (O3_CPU& cpu : env.cpu_view())
        {
            if (next_phase_complete[cpu.cpu] != phase_complete[cpu.cpu])
            {
                for (champsim::operable& op : operables)
                    op.end_phase(cpu.cpu);

                fmt::print("{} finished CPU {} instructions: {} cycles: {} cumulative IPC: {:.4g} (Simulation time: {:%H hr %M min %S sec})\n", phase_name, cpu.cpu,
                    cpu.sim_instr(), cpu.sim_cycle(), std::ceil(cpu.sim_instr()) / std::ceil(cpu.sim_cycle()), elapsed_time());
            }
        }

        phase_complete = next_phase_complete;
    }

    for (O3_CPU& cpu : env.cpu_view())
    {
        fmt::print("{} complete CPU {} instructions: {} cycles: {} cumulative IPC: {:.4g} (Simulation time: {:%H hr %M min %S sec})\n", phase_name, cpu.cpu,
            cpu.sim_instr(), cpu.sim_cycle(), std::ceil(cpu.sim_instr()) / std::ceil(cpu.sim_cycle()), elapsed_time());
    }

    phase_stats stats;
    stats.name = phase.name;

    for (std::size_t i = 0; i < std::size(trace_index); ++i)
        stats.trace_names.push_back(trace_names.at(trace_index.at(i)));

    auto cpus = env.cpu_view();
    std::transform(std::begin(cpus), std::end(cpus), std::back_inserter(stats.sim_cpu_stats), [](const O3_CPU& cpu)
        { return cpu.sim_stats; });
    std::transform(std::begin(cpus), std::end(cpus), std::back_inserter(stats.roi_cpu_stats), [](const O3_CPU& cpu)
        { return cpu.roi_stats; });

    auto caches = env.cache_view();
    std::transform(std::begin(caches), std::end(caches), std::back_inserter(stats.sim_cache_stats), [](const CACHE& cache)
        { return cache.sim_stats; });
    std::transform(std::begin(caches), std::end(caches), std::back_inserter(stats.roi_cache_stats), [](const CACHE& cache)
        { return cache.roi_stats; });

    auto dram = env.dram_view();
    std::transform(std::begin(dram.channels), std::end(dram.channels), std::back_inserter(stats.sim_dram_stats),
        [](const DRAM_CHANNEL& chan)
        { return chan.sim_stats; });
    std::transform(std::begin(dram.channels), std::end(dram.channels), std::back_inserter(stats.roi_dram_stats),
        [](const DRAM_CHANNEL& chan)
        { return chan.roi_stats; });

    return stats;
}

// simulation entry point
std::vector<phase_stats> main(environment& env, std::vector<phase_info>& phases, std::vector<tracereader>& traces)
{
    for (champsim::operable& op : env.operable_view())
        op.initialize();

    std::vector<phase_stats> results;
    for (auto phase : phases)
    {
        auto stats = do_phase(phase, env, traces);
        if (! phase.is_warmup)
            results.push_back(stats);
    }

    return results;
}
} // namespace champsim

#endif // USER_CODES
