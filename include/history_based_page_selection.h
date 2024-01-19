#ifndef HISTORY_H
#define HISTORY_H

#include <cassert>
#include <deque>
#include <iostream>
#include <map>
#include <vector>
#include <queue>
#include <iostream>
#include <fstream>


#include "ChampSim/champsim_constants.h"
#include "ChampSim/channel.h"
#include "ChampSim/util/bits.h"
#include "ProjectConfiguration.h" // User file
#include "Ramulator/Request.h"

/** @note Abbreviation:
 *  FM -> Fast memory (e.g., HBM, DDR4)
 *  SM -> Slow memory (e.g., DDR4, PCM)
*/


#if (MEMORY_USE_OS_TRANSPARENT_MANAGEMENT == ENABLE)

#if (HISTORY_BASED_PAGE_SELECTION == ENABLE)
// #if (IDEAL_LINE_LOCATION_TABLE == ENABLE) || (COLOCATED_LINE_LOCATION_TABLE == ENABLE)

#define COUNTER_WIDTH                         uint8_t
#define COUNTER_MAX_VALUE                     (UINT8_MAX)
#define COUNTER_DEFAULT_VALUE                 (0)

#define HOTNESS_WIDTH                         bool
#define HOTNESS_DEFAULT_VALUE                 (false)

// #define EPOCH_LENGTH                          (10000000) //EPOCH_LENGTH命令ごとにスワップを行う
#define CLEAR_COUNTER_TABLE_EPOCH_NUM         (10) // CLEAR_COUNTER_TABLE_EPOCH_NUM エポック毎にカウンターテーブルの初期化を行う

// overheads
#define OVERHEAD_OF_MIGRATION_PER_PAGE        (5000) //cycles
// #define OVERHEAD_OF_CHANGE_PTE_PER_PAGE        (1000) //cycles
#define OVERHEAD_OF_TLB_SHOOTDOWN_PER_PAGE        (20000) //cycles

#define REMAPPING_LOCATION_WIDTH              uint8_t
#define REMAPPING_LOCATION_WIDTH_BITS         (3) // Default: 3
#define LOCATION_TABLE_ENTRY_WIDTH            uint16_t

#define NUMBER_OF_BLOCK                       (5) // Default: 5

// 0x0538 for the congruence group with 5 members (lines) at most, (000_001_010_011_100_0 = 0x0538)
// [15:13] bit for member 0, [12:10] bit for member 1, [9:7] bit for member 2, [6:4] bit for member 3, [3:1] bit for member 4.
#define LOCATION_TABLE_ENTRY_DEFAULT_VALUE    (0x0538)
#define LOCATION_TABLE_ENTRY_MSB              (UINT16_WIDTH - 1) // MSB -> most significant bit

#define REMAPPING_REQUEST_QUEUE_LENGTH        (4096) // 1024/4096
#define QUEUE_BUSY_DEGREE_THRESHOLD           (1.0f) // 0.8fだった。history based page selectionでは全てのスワップを行う

#define INCOMPLETE_READ_REQUEST_QUEUE_LENGTH  (128)
#define INCOMPLETE_WRITE_REQUEST_QUEUE_LENGTH (128)

#if (BITS_MANIPULATION == DISABLE)
#undef REMAPPING_LOCATION_WIDTH_BITS
#undef NUMBER_OF_BLOCK
#define REMAPPING_LOCATION_WIDTH_BITS (champsim::lg2(64))
#define NUMBER_OF_BLOCK               (35)
#endif // BITS_MANIPULATION

class OS_TRANSPARENT_MANAGEMENT
{
    using channel_type = champsim::channel;
    using request_type = typename channel_type::request_type;

public:
    uint64_t cycle                  = 0;
    COUNTER_WIDTH hotness_threshold = 0;
    uint64_t total_capacity;       // Uint is byte
    uint64_t fast_memory_capacity; // Uint is byte
    uint64_t total_capacity_at_data_block_granularity;
    uint64_t fast_memory_capacity_at_data_block_granularity;
    uint8_t fast_memory_offset_bit; // Address format in the data management granularity
    
    uint64_t instr_count = 0; //if instr_count > epoch_length; スワップを始める 0に初期化する
    uint64_t clear_counter_table_epoch_count = 0;
#if (TEST_HISTORY_BASED_PAGE_SELECTION == ENABLE) // デバッグ用
    bool first_swap = true;
    bool first_swap_epoch = true;   
    bool first_swap_epoch_for_dram_controller = true;
#endif

    std::vector<COUNTER_WIDTH>& counter_table; // A counter for every data block
    std::vector<HOTNESS_WIDTH>& hotness_table; // A hotness bit for every data block, true -> data block is hot, false -> data block is cold.


    std::queue<uint64_t> hotness_data_block_address_queue; //hotなアドレスをhotな順に入れる。

#if (GC_MIGRATION_WITH_GC == ENABLE)
    std::vector<HOTNESS_WIDTH>& hotness_table_with_gc;
    std::queue<uint64_t> hotness_data_block_address_queue_with_gc;
    uint64_t sum_migration_with_gc_count = 0;
#endif // GC_MIGRATION_WITH_GC

    /* Remapping request */
    struct RemappingRequest
    {   
        uint64_t address_in_fm, address_in_sm; // Physical address in fast and slow memories
        // REMAPPING_LOCATION_WIDTH fm_location, sm_location;
        // uint8_t size; // Number of cache lines to remap
    };

    std::deque<RemappingRequest> remapping_request_queue;
    uint64_t remapping_request_queue_congestion;

    // // Scoped enumerations
    // enum class RemappingLocation : REMAPPING_LOCATION_WIDTH
    // {
    //     Zero = 0,
    //     One,
    //     Two,
    //     Three,
    //     Four,
    //     Max = NUMBER_OF_BLOCK
    // };

    // uint8_t congruence_group_msb; // Most significant bit of congruence group, and its address format is in the byte granularity

    /* Remapping table */
// #if (BITS_MANIPULATION == ENABLE)
//     std::vector<LOCATION_TABLE_ENTRY_WIDTH>& line_location_table; // Paper CAMEO: SRAM-Based LLT / Embed LLT in Stacked DRAM
// #else
//     struct LocationTableEntry
//     {
//         REMAPPING_LOCATION_WIDTH location[NUMBER_OF_BLOCK]; // Location field for each line, location[0] is for the line in NM

//         LocationTableEntry()
//         {
//             for (REMAPPING_LOCATION_WIDTH i = 0; i < REMAPPING_LOCATION_WIDTH(RemappingLocation::Max); i++)
//             {
//                 location[i] = i;
//             }
//         };
//     };

//     std::vector<LocationTableEntry>& line_location_table; // Paper CAMEO: SRAM-Based LLT / Embed LLT in Stacked DRAM
// #endif // BITS_MANIPULATION

// For history based
// map<uint64_t physical_page_address, uint64_t hardware_page_address> remapping_table
// map<uint64_t physical_page_address, uint64_t access_count> page_access_count_table
// 元のアドレス->physical_page_address、リマッピング後のアドレス->hardware_page_address
std::vector<std::pair<uint64_t, bool>>& remapping_data_block_table; //index : physical page block address, value : (hardware page block address, valid_bit)

// #if (COLOCATED_LINE_LOCATION_TABLE == ENABLE)
//     /** @brief
//      *  If a memory read request is mapped in slow memory, the memory controller needs first access the fast memory
//      *  to get the Location Entry and Data (LEAD), and then access the slow memory based on that LEAD.
//      */
//     struct ReadRequest
//     {
//         request_type packet;
//         bool fm_access_finish = false; // Whether the access in fast memory is completed.
//     };

//     std::vector<ReadRequest> incomplete_read_request_queue;

//     /** @brief
//      *  If a memory write request is received, the memory controller needs first to figure out where is the right
//      *  place to write. So, the memory controller first access the fast memory to get the Location Entry and Data (LEAD),
//      *  and then write the memory (fast or slow memory).
//      */
//     struct WriteRequest
//     {
//         request_type packet;
//         bool fm_access_finish = false; // Whether the access in fast memory is completed.
//     };

//     std::vector<WriteRequest> incomplete_write_request_queue;
// #endif // COLOCATED_LINE_LOCATION_TABLE

    /* Member functions */
    OS_TRANSPARENT_MANAGEMENT(uint64_t max_address, uint64_t fast_memory_max_address);
    ~OS_TRANSPARENT_MANAGEMENT();

    // Address is physical address and at byte granularity
#if (TRACKING_LOAD_STORE_STATISTICS == ENABLE)
    // Address is physical address and at byte granularity
    bool memory_activity_tracking(uint64_t address, ramulator::Request::Type type, access_type type_origin, float queue_busy_degree);
#else
    // Address is hardware address and at byte granularity
    bool memory_activity_tracking(uint64_t address, ramulator::Request::Type type, float queue_busy_degree);
#endif // TRACKING_LOAD_STORE_STATISTICS

    // Translate the physical address to hardware address
    void physical_to_hardware_address(request_type& packet);
    void physical_to_hardware_address(uint64_t& address);

    bool issue_remapping_request(RemappingRequest& remapping_request);
    bool finish_remapping_request();
    bool epoch_check();
    // bool remapping_request_queue_has_elements();

    // Detect cold data block
    void cold_data_detection();

// #if (COLOCATED_LINE_LOCATION_TABLE == ENABLE)
//     bool finish_fm_access_in_incomplete_read_request_queue(uint64_t h_address);
//     bool finish_fm_access_in_incomplete_write_request_queue(uint64_t h_address);
// #endif // COLOCATED_LINE_LOCATION_TABLE
#if(GC_TRACE == ENABLE)
#if (GC_MIGRATION_WITH_GC == ENABLE)
#if (GC_MARKED_OBJECT == ENABLE)
    bool choose_hotpage_with_sort_with_gc(std::vector<std::uint64_t>); 
#else // GC_MARKED_OBJECT
    bool choose_hotpage_with_sort_with_gc_unmarked(std::vector<std::uint64_t>);
#endif // GC_MARKED_OBJECT
    bool add_new_remapping_request_to_queue_with_gc(std::vector<std::uint64_t>);
    uint64_t migration_all_start_with_gc();
    void initialize_hotness_table_with_gc(std::vector<HOTNESS_WIDTH>& table);
#endif // GC_MIGRATION_WITH_GC
#endif // GC_TRACE
private:
    // Evict cold data block
    bool cold_data_eviction(uint64_t source_address, float queue_busy_degree);

    // Add new remapping request into the remapping_request_queue
    bool enqueue_remapping_request(RemappingRequest& remapping_request);

    bool choose_hotpage_with_sort();

    bool add_new_remapping_request_to_queue(float);
    // RemappingRequest choose_swap_page_in_fm();
    void initialize_counter_table(std::vector<COUNTER_WIDTH>& table);

    void initialize_hotness_table(std::vector<HOTNESS_WIDTH>& table);
};

#endif // HISTORY_BASED_PAGE_SELECTION

#endif // MEMORY_USE_OS_TRANSPARENT_MANAGEMENT
#endif // HISTORY_H
