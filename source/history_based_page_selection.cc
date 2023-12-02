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
  hotness_table(*(new std::vector<HOTNESS_WIDTH>(max_address >> DATA_MANAGEMENT_OFFSET_BITS, HOTNESS_DEFAULT_VALUE))),
  remapping_data_block_table(*(new std::vector<uint64_t>(max_address >> DATA_MANAGEMENT_OFFSET_BITS)))
{
    hotness_threshold                            = HOTNESS_THRESHOLD; //まずは1に設定しよう
    remapping_request_queue_congestion           = 0;

    // remapping_data_block_tableの初期化
    // index : physical page block address, value : hardware page block address
    for(uint64_t i=0;i < max_address >> DATA_MANAGEMENT_OFFSET_BITS; i++) {
        remapping_data_block_table.at(i) = i;
    }

    printf("hotness threshold = %d\n", hotness_threshold);
};

OS_TRANSPARENT_MANAGEMENT::~OS_TRANSPARENT_MANAGEMENT() //良く考えてない
{
    output_statistics.remapping_request_queue_congestion = remapping_request_queue_congestion; //いらないかも

    delete &counter_table;
    delete &hotness_table;
    delete &remapping_data_block_table;
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
            bool is_there_coldpage_in_fm = false;
            // 高速メモリにあるコールドページを検索
            for(uint64_t j = 0; j < fast_memory_capacity_at_data_block_granularity; j++) {
                if(hotness_table.at(j) == false) {
                    is_there_coldpage_in_fm = true;
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
            // 高速メモリにコールドページがない場合、スワップ終了
            if(!is_there_coldpage_in_fm) {
                std::cout << "All fast memory pages are hot" << std::endl;
                break;
            }
        }
    }
    return true;
}

void OS_TRANSPARENT_MANAGEMENT::physical_to_hardware_address(request_type& packet)
{
    uint64_t data_block_address        = packet.address >> DATA_MANAGEMENT_OFFSET_BITS;
    uint64_t remapping_data_block_address = remapping_data_block_table.at(data_block_address);

    packet.h_address = champsim::replace_bits(remapping_data_block_address << DATA_MANAGEMENT_OFFSET_BITS, packet.address, DATA_MANAGEMENT_OFFSET_BITS - 1);
};

void OS_TRANSPARENT_MANAGEMENT::physical_to_hardware_address(uint64_t& address)
{
    uint64_t data_block_address        = address >> DATA_MANAGEMENT_OFFSET_BITS;
    uint64_t remapping_data_block_address = remapping_data_block_table.at(data_block_address);

    address = champsim::replace_bits(remapping_data_block_address << DATA_MANAGEMENT_OFFSET_BITS, address, DATA_MANAGEMENT_OFFSET_BITS - 1);
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
    if (remapping_request_queue.empty() == false)
    {
        RemappingRequest remapping_request = remapping_request_queue.front();
        remapping_request_queue.pop_front();

        uint64_t data_block_address_in_fm        = remapping_request.address_in_fm >> DATA_MANAGEMENT_OFFSET_BITS;
        uint64_t data_block_address_in_sm        = remapping_request.address_in_sm >> DATA_MANAGEMENT_OFFSET_BITS;
        
        remapping_data_block_table.at(data_block_address_in_fm) = data_block_address_in_sm;
        remapping_data_block_table.at(data_block_address_in_sm) = data_block_address_in_fm;

        // Sanity check
        if (data_block_address_in_fm == data_block_address_in_sm)
        {
            std::cout << __func__ << ": read remapping location error." << std::endl;
            abort();
        }
    }
    else
    {
        std::cout << __func__ << ": remapping error." << std::endl;
        assert(false);
        return false; // Error
    }
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
    uint64_t data_block_address        = remapping_request.address_in_fm >> DATA_MANAGEMENT_OFFSET_BITS;
    // Check duplicated remapping request in remapping_request_queue
    // If duplicated remapping requests exist, we won't add this new remapping request into the remapping_request_queue.
    bool duplicated_remapping_request  = false;
    for (uint64_t i = 0; i < remapping_request_queue.size(); i++)
    {
        uint64_t data_block_address_to_check        = remapping_request_queue[i].address_in_fm >> DATA_MANAGEMENT_OFFSET_BITS;

        if (data_block_address == data_block_address_to_check)
        {
            duplicated_remapping_request = true; // Find a duplicated remapping request
            break;
        }
    }

    if (duplicated_remapping_request == false)
    {
        if (remapping_request_queue.size() < REMAPPING_REQUEST_QUEUE_LENGTH)
        {
            if (remapping_request.address_in_fm == remapping_request.address_in_sm) // Check
            {
                std::cout << __func__ << ": add new remapping request error 2." << std::endl;
                abort();
            }

            // Enqueue a remapping request
            remapping_request_queue.push_back(remapping_request);
        }
        else
        {
            // std::cout << __func__ << ": remapping_request_queue is full." << std::endl;
            std::cout << "Warning for history base :: remapping_request_queue is full. REMAPPING_REQUEST_QUEUE_LENGTH might be too small." << std::endl;
            remapping_request_queue_congestion++;
        }
    }
    else
    {
        return false;
    }

    return true;
}
#endif // HISTORY_BASED_PAGE_SELECTION
#endif // MEMORY_USE_OS_TRANSPARENT_MANAGEMENT