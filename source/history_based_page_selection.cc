// #include "history_based_page_selection.h"
#include "os_transparent_management.h"

#if (MEMORY_USE_OS_TRANSPARENT_MANAGEMENT == ENABLE)

#if (HISTORY_BASED_PAGE_SELECTION == ENABLE)
OS_TRANSPARENT_MANAGEMENT::OS_TRANSPARENT_MANAGEMENT(uint64_t max_address, uint64_t fast_memory_max_address)
: total_capacity(max_address), fast_memory_capacity(fast_memory_max_address),
  total_capacity_at_data_block_granularity(max_address >> DATA_MANAGEMENT_OFFSET_BITS),
  fast_memory_capacity_at_data_block_granularity(fast_memory_max_address >> DATA_MANAGEMENT_OFFSET_BITS),
  fast_memory_offset_bit(champsim::lg2(fast_memory_max_address)), // Note here only support integers of 2's power.
  counter_table(*(new std::vector<COUNTER_WIDTH>(max_address >> DATA_MANAGEMENT_OFFSET_BITS, COUNTER_DEFAULT_VALUE))),
  hotness_table(*(new std::vector<HOTNESS_WIDTH>(max_address >> DATA_MANAGEMENT_OFFSET_BITS, HOTNESS_DEFAULT_VALUE)))
{
    hotness_threshold                            = HOTNESS_THRESHOLD; //まずは1に設定しよう
    remapping_request_queue_congestion           = 0; //良くわからない、いらないかも

    printf("hotness threshold = %d\n", hotness_threshold);
};

OS_TRANSPARENT_MANAGEMENT::~OS_TRANSPARENT_MANAGEMENT() //良く考えてない
{
    output_statistics.remapping_request_queue_congestion = remapping_request_queue_congestion; //いらないかも

    delete &counter_table;
    delete &hotness_table;
};

bool OS_TRANSPARENT_MANAGEMENT::memory_activity_tracking(uint64_t address, ramulator::Request::Type type, float queue_busy_degree)
{
    if (address >= total_capacity)
    {
        std::cout << __func__ << ": address input error." << std::endl;
        return false;
    }

    uint64_t data_block_address        = address >> DATA_MANAGEMENT_OFFSET_BITS; 

    if (DATA_MANAGEMENT_OFFSET_BITS != champsim::lg2(((4096u)))) {
        std::cout << "DATA_MANAGEMENT_GRANULARITY is not page size(4KB)\n" << std::endl;
        abort();
    }

    if (type == ramulator::Request::Type::READ) // For read request
    {
        if (counter_table.at(data_block_address) < COUNTER_MAX_VALUE)
        {
            counter_table[data_block_address]++; // Increment its counter
        }

        if (counter_table.at(data_block_address) >= hotness_threshold)
        {
            hotness_table.at(data_block_address) = true; // Mark hot data block
        }
    }
    else if (type == ramulator::Request::Type::WRITE) // For write request
    {
        if (counter_table.at(data_block_address) < COUNTER_MAX_VALUE)
        {
            counter_table[data_block_address]++; // Increment its counter
        }

        if (counter_table.at(data_block_address) >= hotness_threshold)
        {
            hotness_table.at(data_block_address) = true; // Mark hot data block
        }
    }
    else
    {
        std::cout << __func__ << ": type input error." << std::endl;
        assert(false);
    }

    epoch_count++;
    if(epoch_count >= EPOCH_LENGTH) {
        add_new_remapping_request_to_queue(queue_busy_degree);
        epoch_count = 0;
    }

    return true;
}

bool OS_TRANSPARENT_MANAGEMENT::add_new_remapping_request_to_queue(float queue_busy_degree)
{
    for(uint64_t i = fast_memory_capacity_at_data_block_granularity; i < total_capacity_at_data_block_granularity; i++) {
        // 低速メモリにあるホットページを発見
        if(hotness_table.at(i) == true) {
            uint64_t tmp_hotpage_data_block_address_in_sm = i;
            uint64_t tmp_coldpage_data_block_address_in_fm;
            // 高速メモリにあるコールドページを検索
            for(uint64_t j = 0; j < fast_memory_capacity_at_data_block_granularity; j++) {
                if(hotness_table.at(j) == false) {
                    tmp_coldpage_data_block_address_in_fm = j;
                    hotness_table.at(j) = true; //これからhotpageが入るのでhotに変更
                    RemappingRequest remapping_request;
                    remapping_request.address_in_fm = tmp_coldpage_data_block_address_in_fm << DATA_MANAGEMENT_OFFSET_BITS;
                    remapping_request.address_in_sm = tmp_hotpage_data_block_address_in_sm << DATA_MANAGEMENT_OFFSET_BITS;
                    hotness_table.at(i) = false; //coldpageが入るので変更
                    if (queue_busy_degree <= QUEUE_BUSY_DEGREE_THRESHOLD)
                    {
                        enqueue_remapping_request(remapping_request);
                    }
                    break;
                }
            }
        }
    }
    std::cout << "add_new_remapping_request_to_queue()" << std::endl;
    return true;
}

void OS_TRANSPARENT_MANAGEMENT::physical_to_hardware_address(request_type& packet)
{
    std::cout << "physical_to_hardware_address" << std::endl;
};

void OS_TRANSPARENT_MANAGEMENT::physical_to_hardware_address(uint64_t& address)
{
    std::cout << "physical_to_hardware_address" << std::endl;
};

bool OS_TRANSPARENT_MANAGEMENT::issue_remapping_request(RemappingRequest& remapping_request)
{
    if (remapping_request_queue.empty() == false)
    {
        remapping_request = remapping_request_queue.front();
        return true;
    }

    return false;
};

bool OS_TRANSPARENT_MANAGEMENT::finish_remapping_request()
{
    std::cout << "finish_remapping_request()" << std::endl;
    return true;
}

void OS_TRANSPARENT_MANAGEMENT::cold_data_detection()
{
    cycle++;
}

bool OS_TRANSPARENT_MANAGEMENT::cold_data_eviction(uint64_t source_address, float queue_busy_degree)
{
    return false;
}

bool OS_TRANSPARENT_MANAGEMENT::enqueue_remapping_request(RemappingRequest& remapping_request)
{
    std::cout << "enqueue_remapping_request" << std::endl;
    return true;
}
#endif // HISTORY_BASED_PAGE_SELECTION
#endif // MEMORY_USE_OS_TRANSPARENT_MANAGEMENT