#ifndef __PROJECTCONFIGURATION_H
#define __PROJECTCONFIGURATION_H

#define ENABLE       (1)
#define DISABLE      (0)

#define DEBUG_PRINTF (DISABLE)
#define USER_CODES   (ENABLE)
//  test
#define TEST_HISTORY_BASED_PAGE_SELECTION (DISABLE) // History based page selectionのswapが正常に行われているかどうかをチェックする
/* Functionality options */
#if (USER_CODES == ENABLE)
/** Main functionalities selection */
#define USE_OPENMP                           (ENABLE)  // Whether use OpenMP to speedup the simulation
#define USE_VCPKG                            (ENABLE)  // Whether use Vcpkg (Not work currently)
#define RAMULATOR                            (ENABLE)  // Whether use ramulator, assuming ramulator uses addresses at byte granularity and returns data at cache line granularity.
#define MEMORY_USE_HYBRID                    (ENABLE) // Whether use hybrid memory system instead of single memory systems
#define PRINT_STATISTICS_INTO_FILE           (ENABLE)  // Whether print simulation statistics into files
#define PRINT_MEMORY_TRACE                   (DISABLE)  // Whether print memory trace into files
#define MEMORY_USE_SWAPPING_UNIT             (ENABLE)  // Whether memory controller uses swapping unit to swap data (data swapping overhead is considered)
#define MEMORY_USE_OS_TRANSPARENT_MANAGEMENT (ENABLE)  // Whether memory controller uses OS-transparent management designs to simulate the memory system instead of static (no-migration) methods
#define CPU_USE_MULTIPLE_CORES               (DISABLE) // Whether CPU uses multiple cores to run simulation (go to include/ChampSim/champsim_constants.h to check related parameters)

/** Configuration for GC trace */
#define GC_TRACE           (ENABLE) // traceがGCを含んでいるならENABLE
#define GC_MARKED_OBJECT           (DISABLE) // ChampSimがmarked_pagesを読むのか（ENABLE）、unmarked_pagesを読むのか
#define GC_MIGRATION_WITH_GC       (ENABLE)
#if (GC_MIGRATION_WITH_GC == ENABLE)
#define GC_TRACE (ENABLE) //GC_TRACEを強制的にENABLEに
// #define MIN_ACCESS_COUNT_GCM (128) // GCマイグレーション時にこの値を超えていないとマイグレーションしない
#define MAX_PAGES_GCM (150) // GCマイグレーションする最大ページ数
#endif
#define MAX_PAGES_MIG (9999) // マイグレーションする最大ページ数

#define DISTANCE_MIGRATION (3000000) // マイグレーションの最低間隔(命令数）

/** taiga debug */
#define CHECK_INSTR_ADDRESS (DISABLE) //命令がアクセスする物理アドレスを出力
#define PRINT_V_ADDRESS (DISABLE) //仮想アドレスを出力（失敗）
#define PRINT_V_ADDRESS_TRACE (DISABLE) //仮想アドレスを出力
#define TEST_HISTORY_WITH_GC (DISABLE) // unmarked_pagesを出力, マイグレーションページを出力, gc with マイグレーションページを出力

/** Configuration for hybrid memory systems */
#if (MEMORY_USE_HYBRID == ENABLE)
#define NUMBER_OF_MEMORIES (2U) // We use two memories for hybrid memory system.
#define MEMORY_NUMBER_ONE  (0u)
#define MEMORY_NUMBER_TWO  (1u)

#define ADD_HBM_128MB      (DISABLE) // need changing HBM-config.cfg to change HBM capacity
#define ADD_HBM_64MB      (DISABLE) // need changing HBM-config.cfg to change HBM capacity
#define ADD_HBM_8MB      (ENABLE) // need changing HBM-config.cfg to change HBM capacity
#define ADD_HBM_1MB      (DISABLE) // need changing HBM-config.cfg to change HBM capacity. It has issues with CAMEO and similar tools; they are not functioning properly.
#else
#define NUMBER_OF_MEMORIES (1u)
#endif // MEMORY_USE_HYBRID

/** Configuration for swapping unit in the memory controller */
#if (MEMORY_USE_SWAPPING_UNIT == ENABLE)
#define SWAPPING_BUFFER_ENTRY_NUMBER (64)
#define SWAPPING_SEGMENT_ONE         (0)
#define SWAPPING_SEGMENT_TWO         (1)
#define SWAPPING_SEGMENT_NUMBER      (2)
#define TEST_SWAPPING_UNIT           (DISABLE) /** @todo Use unit testing to test this unit */

#endif // MEMORY_USE_SWAPPING_UNIT

/* Research proposal selection */
#if (MEMORY_USE_OS_TRANSPARENT_MANAGEMENT == ENABLE)
#define IDEAL_LINE_LOCATION_TABLE      (DISABLE)
#define COLOCATED_LINE_LOCATION_TABLE  (DISABLE)
#define IDEAL_VARIABLE_GRANULARITY     (DISABLE)
#define IDEAL_SINGLE_MEMPOD            (DISABLE)
#define HISTORY_BASED_PAGE_SELECTION   (ENABLE)
#define NO_MIGRATION (DISABLE) // 応急処置　HISTORY_BASED_PAGE_SELECTIOINの閾値を無限にする

#define TRACKING_LOAD_STORE_STATISTICS (DISABLE) //HISTORY BASEDでは使えない

#if (IDEAL_LINE_LOCATION_TABLE == DISABLE) && (COLOCATED_LINE_LOCATION_TABLE == DISABLE) && (IDEAL_VARIABLE_GRANULARITY == DISABLE) && (IDEAL_SINGLE_MEMPOD == DISABLE) && (HISTORY_BASED_PAGE_SELECTION == DISABLE)
#define NO_METHOD_FOR_RUN_HYBRID_MEMORY (ENABLE)
#endif // IDEAL_LINE_LOCATION_TABLE, COLOCATED_LINE_LOCATION_TABLE, IDEAL_VARIABLE_GRANULARITY, IDEAL_SINGLE_MEMPOD

#if (TRACKING_LOAD_STORE_STATISTICS == ENABLE) // Note: it might be better become part of configurations of TRACKING_LOAD_STORE_STATISTICS like IDEAL_SINGLE_MEMPOD in line 71
/* Option for research */
#define TRACKING_LOAD_ONLY (ENABLE)
#define TRACKING_READ_ONLY (ENABLE)
#endif // TRACKING_LOAD_STORE_STATISTICS

/** Configuration for each research proposal */
#if (IDEAL_LINE_LOCATION_TABLE == ENABLE) || (COLOCATED_LINE_LOCATION_TABLE == ENABLE)
#define HOTNESS_THRESHOLD (1u)
#define BITS_MANIPULATION (DISABLE)

#elif (IDEAL_VARIABLE_GRANULARITY == ENABLE)
#define HOTNESS_THRESHOLD            (1u) // Default: 1/4
#define DATA_EVICTION                (ENABLE)
#define FLEXIBLE_DATA_PLACEMENT      (ENABLE)
#define STATISTICS_INFORMATION       (ENABLE)
#define FLEXIBLE_GRANULARITY         (ENABLE)
#define IMMEDIATE_EVICTION           (DISABLE)
#define COLD_DATA_DETECTION_IN_GROUP (DISABLE)

#elif (IDEAL_SINGLE_MEMPOD == ENABLE)
#define PRINT_SWAPS_PER_EPOCH_MEMPOD (DISABLE)

#elif (HISTORY_BASED_PAGE_SELECTION == ENABLE)
#define EPOCH_LENGTH                          (100000000) //EPOCH_LENGTH命令ごとにスワップを行う
#define CLEAR_COUNTER_TABLE_EPOCH_NUM         (1) // CLEAR_COUNTER_TABLE_EPOCH_NUM エポック毎にカウンターテーブルの初期化を行う
// overheads
#define OVERHEAD_OF_MIGRATION_PER_PAGE        (5000) //cycles
// #define OVERHEAD_OF_CHANGE_PTE_PER_PAGE        (1000) //cycles
#define OVERHEAD_OF_TLB_SHOOTDOWN_PER_PAGE        (14000) //cycles
#if (GC_MIGRATION_WITH_GC == ENABLE)
#define HOTNESS_THRESHOLD_WITH_GC (128u) // gcと同時にマイグレーションするときのhotness閾値
#define COLDNESS_THRESHOLD_WITH_GC (1u)
#endif // GC_MIGRATION_WITH_GC
#if (NO_MIGRATION==ENABLE) //マイグレーションを起こさない
#define HOTNESS_THRESHOLD (32768u)
#else // NO_MIGRATION
#define HOTNESS_THRESHOLD (1280u)
#endif //NO_MIGRATION
#define COLDNESS_THRESHOLD (65u)
#elif (NO_METHOD_FOR_RUN_HYBRID_MEMORY == ENABLE)
#define HOTNESS_THRESHOLD (1u) // 使われない
#endif // IDEAL_LINE_LOCATION_TABLE, COLOCATED_LINE_LOCATION_TABLE, IDEAL_VARIABLE_GRANULARITY, IDEAL_SINGLE_MEMPOD, HISTORY_BASED_PAGE_SELECTION

// Check
// #if (NO_METHOD_FOR_RUN_HYBRID_MEMORY == ENABLE)
// #error OS-transparent management designs need to be enabled.
// #endif // IDEAL_LINE_LOCATION_TABLE, COLOCATED_LINE_LOCATION_TABLE

#endif // MEMORY_USE_OS_TRANSPARENT_MANAGEMENT

#if (PRINT_MEMORY_TRACE == ENABLE)
#define CONTINUOUS_ADDRESS (ENABLE)
#endif // PRINT_MEMORY_TRACE

// Data block management granularity
#define DATA_GRANULARITY_64B   (64u)
#define DATA_GRANULARITY_128B  (128u)
#define DATA_GRANULARITY_256B  (256u)
#define DATA_GRANULARITY_512B  (512u)
#define DATA_GRANULARITY_1024B (1024u)
#define DATA_GRANULARITY_2048B (2048u)
#define DATA_GRANULARITY_4096B (4096u)

#if (IDEAL_SINGLE_MEMPOD == DISABLE)
#if (IDEAL_LINE_LOCATION_TABLE == ENABLE) || (COLOCATED_LINE_LOCATION_TABLE == ENABLE)
#define DATA_MANAGEMENT_GRANULARITY (DATA_GRANULARITY_64B) // Default: DATA_GRANULARITY_64B
#elif (IDEAL_VARIABLE_GRANULARITY == ENABLE)
#define DATA_MANAGEMENT_GRANULARITY (DATA_GRANULARITY_4096B)
#define DATA_LINE_OFFSET_BITS       (champsim::lg2(DATA_GRANULARITY_64B))
#elif (HISTORY_BASED_PAGE_SELECTION == ENABLE)
#define DATA_MANAGEMENT_GRANULARITY (DATA_GRANULARITY_4096B)
#else
#define DATA_MANAGEMENT_GRANULARITY (DATA_GRANULARITY_64B)
#endif // IDEAL_LINE_LOCATION_TABLE, COLOCATED_LINE_LOCATION_TABLE, IDEAL_VARIABLE_GRANULARITY

#define DATA_MANAGEMENT_OFFSET_BITS    (champsim::lg2(DATA_MANAGEMENT_GRANULARITY)) // Data management granularity means how the hardware cluster the data
#define DATA_GRANULARITY_IN_CACHE_LINE (DATA_MANAGEMENT_GRANULARITY / DATA_GRANULARITY_64B)
#endif // IDEAL_SINGLE_MEMPOD

#if (USE_OPENMP == ENABLE)
#define SET_THREADS_NUMBER (6)
#endif // USE_OPENMP

#define KiB (1024ul) // Unit is byte
#define MiB (KiB * KiB)
#define GiB (MiB * KiB)

/* Header */
// Standard libraries
#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <array>
#include <cassert>
#include <cstdio>
#include <string>

#if (USE_OPENMP == ENABLE)
#include <omp.h>
#endif // USE_OPENMP

/* Macro */

/* Type */

/* Prototype */

// Data output class
class DATA_OUTPUT
{
public:
    const std::string data_name;
    FILE* file_handler = NULL;
    char* file_name    = NULL;
    const std::string file_extension;

    DATA_OUTPUT(std::string v1, std::string v2);
    ~DATA_OUTPUT();

    void output_file_initialization(char** string_array, uint32_t number, uint64_t warmup_instr=0, uint64_t simulation_instr=0);
};

// Memory trace output class
class MEMORY_TRACE : public DATA_OUTPUT
{
public:
    MEMORY_TRACE(std::string v1, std::string v2);
    MEMORY_TRACE(std::string v1, std::string v2, char** string_array, uint32_t number, uint64_t warmup_instr, uint64_t simulation_instr);
    ~MEMORY_TRACE();

    void output_memory_trace_hexadecimal(uint64_t address, char type);
};

// Simulator statistics output class
class SIMULATOR_STATISTICS : public DATA_OUTPUT
{
public:
#define PAGE_TABLE_LEVEL_NUMBER (5u)

    std::array<uint64_t, PAGE_TABLE_LEVEL_NUMBER> valid_pte_count = {0};
    uint64_t virtual_page_count;

    uint64_t read_request_in_memory, read_request_in_memory2;
    uint64_t write_request_in_memory, write_request_in_memory2;

    // taiga debug
    uint64_t migration_read_request_in_memory, migration_read_request_in_memory2, gcmigration_read_request_in_memory, gcmigration_read_request_in_memory2, nomigration_read_request_in_memory, nomigration_read_request_in_memory2;
    uint64_t migration_write_request_in_memory, migration_write_request_in_memory2, gcmigration_write_request_in_memory, gcmigration_write_request_in_memory2,nomigration_write_request_in_memory, nomigration_write_request_in_memory2;
    // taiga debug

#if (TRACKING_LOAD_STORE_STATISTICS == ENABLE)
    uint64_t load_request_in_memory, load_request_in_memory2;
    uint64_t store_request_in_memory, store_request_in_memory2;
#endif // TRACKING_LOAD_STORE_STATISTICS

    uint64_t swapping_count;
    uint64_t swapping_traffic_in_bytes;
    uint64_t migration_cycles;
#if (GC_TRACE == ENABLE)
#if (GC_MIGRATION_WITH_GC == ENABLE)
    uint64_t sum_migration_with_gc_count;
    uint64_t migration_cycles_with_gc;
    uint64_t gcmigration_tlb_overhead, gcmigration_sum_overhead_without_tlb;
#endif // GC_MIGRATION_WITH_GC
#endif // GC_TRACE
    uint64_t remapping_request_queue_congestion;

#if (IDEAL_VARIABLE_GRANULARITY == ENABLE)
    uint64_t no_free_space_for_migration;
    uint64_t no_invalid_group_for_migration;
    uint64_t unexpandable_since_start_address;
    uint64_t unexpandable_since_no_invalid_group;
    uint64_t data_eviction_success, data_eviction_failure;
    uint64_t uncertain_counter;
#endif // IDEAL_VARIABLE_GRANULARITY

    SIMULATOR_STATISTICS(std::string v1, std::string v2);
    SIMULATOR_STATISTICS(std::string v1, std::string v2, char** string_array, uint32_t number, uint64_t warmup_instr, uint64_t simulation_instr);
    ~SIMULATOR_STATISTICS();

private:
    void statistics_initialization();
};

extern MEMORY_TRACE output_memorytrace;
extern SIMULATOR_STATISTICS output_statistics;

static std::string output_file_path_statistic = "/home/funkytaiga/tmp_champ/ChampSim-Ramulator/statistics/";

/* Variable */

/* Function */

#endif // USER_CODES

/** @note
 *  600.perlbench_s-210B.champsimtrace.xz
 *  602.gcc_s-734B.champsimtrace.xz
 *  603.bwaves_s-3699B.champsimtrace.xz
 *  605.mcf_s-665B.champsimtrace.xz
 *  607.cactuBSSN_s-2421B.champsimtrace.xz
 *  619.lbm_s-4268B.champsimtrace.xz
 *  620.omnetpp_s-874B.champsimtrace.xz
 *  621.wrf_s-575B.champsimtrace.xz
 *  623.xalancbmk_s-700B.champsimtrace.xz
 *  625.x264_s-18B.champsimtrace.xz
 *  627.cam4_s-573B.champsimtrace.xz
 *  628.pop2_s-17B.champsimtrace.xz
 *  631.deepsjeng_s-928B.champsimtrace.xz has 2000000000 PIN traces
 *  638.imagick_s-10316B.champsimtrace.xz
 *  641.leela_s-800B.champsimtrace.xz
 *  644.nab_s-5853B.champsimtrace.xz
 *  648.exchange2_s-1699B.champsimtrace.xz
 *  649.fotonik3d_s-1176B.champsimtrace.xz
 *  654.roms_s-842B.champsimtrace.xz
 *  657.xz_s-3167B.champsimtrace.xz
 *  Protection: 1989/4/15-1989/6/4, 39'54'12'N 116'23'30'E
 *
 */

#endif
