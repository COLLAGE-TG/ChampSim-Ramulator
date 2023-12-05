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
        std::cout << __func__ << ": h_address input error." << std::endl;
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
    }
    else if (type == ramulator::Request::Type::WRITE) // For write request
    {
        if (counter_table.at(data_block_address) < COUNTER_MAX_VALUE)
        {
            counter_table[data_block_address]++; // Increment its counter
        }
    }
    else
    {
        std::cout << __func__ << ": type input error." << std::endl;
        assert(false);
    }

    epoch_count++;
    if(epoch_count >= EPOCH_LENGTH) {
        choose_hotpage_with_sort();
        add_new_remapping_request_to_queue(queue_busy_degree);
        epoch_count = 0;
        clear_counter_table_epoch_count++;
        initialize_hotness_table(hotness_table);
    }
    // カウンターテーブルの初期化
    // CLEAR_COUNTER_TABLE_EPOCK_NUMエポック毎に行う
    if(clear_counter_table_epoch_count >= CLEAR_COUNTER_TABLE_EPOCK_NUM) {
        initialize_counter_table(counter_table);
        clear_counter_table_epoch_count = 0;
    }

    return true;
} 

bool OS_TRANSPARENT_MANAGEMENT::choose_hotpage_with_sort() 
{
    std::vector<std::pair<uint64_t, uint64_t>> tmp_pages_and_count(counter_table.size()); //this.first = （data_block_address）, this.second = （counter）
    // counter_tableを複製
    for(uint64_t i =0; i < tmp_pages_and_count.size(); i++) {
        tmp_pages_and_count.at(i).first = i;
        tmp_pages_and_count.at(i).second = counter_table.at(i);
    }

    // ペアをsecond要素で降順ソート
    std::sort(tmp_pages_and_count.begin(), tmp_pages_and_count.end(), [](const auto& a, const auto& b) {
        return a.second > b.second;
    });

    // hotness tableを更新
    for(uint64_t i = 0; i < fast_memory_capacity_at_data_block_granularity; i++) {
        // カウンターが0なら終了
        if(tmp_pages_and_count.at(i).second == 0) {
            break;
        }
        uint64_t tmp_hotpage_data_block_address = tmp_pages_and_count.at(i).first;
        hotness_table.at(tmp_hotpage_data_block_address) = true;
        hotness_address_queue.push(tmp_hotpage_data_block_address); //hotな順にキューに入れていく。
    }

    return true;
}

bool OS_TRANSPARENT_MANAGEMENT::add_new_remapping_request_to_queue(float queue_busy_degree)
{
    for(uint64_t data_block_address = 0; data_block_address < total_capacity_at_data_block_granularity; data_block_address++) {
        // キューが空なら終了
        if(hotness_address_queue.empty()) {
            break;
        }
        // コールドページなら
        if(hotness_table.at(data_block_address) == false) {
            //物理アドレスからハードウェアアドレスに変換
            uint64_t h_data_block_address = remapping_data_block_table.at(data_block_address);
            if(h_data_block_address < fast_memory_capacity_at_data_block_granularity) { // コールドかつ高速メモリにあるなら
                uint64_t hotness_data_block_address = hotness_address_queue.front();
                RemappingRequest remapping_request;
                remapping_request.address_in_fm = data_block_address << DATA_MANAGEMENT_OFFSET_BITS;
                remapping_request.address_in_sm = hotness_data_block_address << DATA_MANAGEMENT_OFFSET_BITS;
                // check
                if(hotness_table.at(hotness_data_block_address) == false) {
                    std::cout << "ERROR : hotpage is not hot" << std::endl;
                    abort();

                }
                if(remapping_request.address_in_fm == remapping_request.address_in_sm) {
                    std::cout << "ERROR : remapping_request.address_in_fm == remapping_request.address_in_sm" << std::endl;
                    abort();
                }
                hotness_address_queue.pop();
                if (queue_busy_degree <= QUEUE_BUSY_DEGREE_THRESHOLD)
                {
                    enqueue_remapping_request(remapping_request);
                }
                break;
            }
        }
        
    }
    // hotness_address_queue　初期化
    // std::queue<uint64_t> hotness_address_queue;
    while(!hotness_address_queue.empty()) {
        hotness_address_queue.pop();
    }

    return true;
}

// bool OS_TRANSPARENT_MANAGEMENT::add_new_remapping_request_to_queue(float queue_busy_degree)
// {
//     for(uint64_t i = fast_memory_capacity_at_data_block_granularity; i < total_capacity_at_data_block_granularity; i++) {
//         // 低速メモリにあるホットページを発見
//         if(hotness_table.at(i) == true) {
//             uint64_t tmp_h_data_block_address = remapping_data_block_table.at(i);
//             uint64_t tmp_hotpage_data_block_address_in_sm = i;
//             uint64_t tmp_coldpage_data_block_address_in_fm;
//             bool is_there_coldpage_in_fm = false;
//             // 高速メモリにあるコールドページを検索
//             for(uint64_t j = 0; j < fast_memory_capacity_at_data_block_granularity; j++) {
//                 if(hotness_table.at(j) == false) {
//                     is_there_coldpage_in_fm = true;
//                     tmp_coldpage_data_block_address_in_fm = j;
//                     RemappingRequest remapping_request;
//                     remapping_request.address_in_fm = tmp_coldpage_data_block_address_in_fm << DATA_MANAGEMENT_OFFSET_BITS;
//                     remapping_request.address_in_sm = tmp_hotpage_data_block_address_in_sm << DATA_MANAGEMENT_OFFSET_BITS;
//                     // counter_tableも入れ替え
//                     int8_t tmp_counter = counter_table.at(i);
//                     counter_table.at(i) = counter_table.at(j);
//                     counter_table.at(j) = tmp_counter;
//                     // hotness_tableも入れ替え
//                     hotness_table.at(j) = true; //これからhotpageが入るのでhotに変更
//                     hotness_table.at(i) = false; //coldpageが入るので変更
//                     if (queue_busy_degree <= QUEUE_BUSY_DEGREE_THRESHOLD)
//                     {
//                         enqueue_remapping_request(remapping_request);
//                     }
//                     break;
//                 }
//             }
//             // 高速メモリにコールドページがない場合、スワップ終了
//             if(!is_there_coldpage_in_fm) {
//                 std::cout << "All fast memory pages are hot" << std::endl;
//                 break;
//             }
//         }
//     }
//     return true;
// }

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


bool OS_TRANSPARENT_MANAGEMENT::epoch_check()
{   
    if(epoch_count == EPOCH_LENGTH) return true;
    else if(epoch_count > EPOCH_LENGTH) { // チェック
        std::cout << "epoch_length > EPOCH_LENGTH" << std::endl; 
        std::cout << "epoch_length = " << epoch_count << std::endl;
        abort();
    }
    else if(epoch_count < EPOCH_LENGTH) return false;
    else { // 通常はここには来ない
            std::cout << "epoch_lengthの値が異常です。" << std::endl; 
            std::cout << "epoch_length = " << epoch_count << std::endl;
            abort();
    }
}

bool OS_TRANSPARENT_MANAGEMENT::remapping_request_queue_has_elements()
{
    if(remapping_request_queue.empty() == false) return true;
    else if(remapping_request_queue.empty() == true) return false;
    else {
        std::cout << "ERROR : remapping_request_queue_has_elements()" << std::endl;
        abort();
    }
}

void OS_TRANSPARENT_MANAGEMENT::cold_data_detection()
{
    cycle++;
}

bool OS_TRANSPARENT_MANAGEMENT::cold_data_eviction(uint64_t source_address, float queue_busy_degree)
{
    return false;
}

// remapping_requestに入れるアドレスは物理アドレス
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

void OS_TRANSPARENT_MANAGEMENT::initialize_counter_table(std::vector<COUNTER_WIDTH>& table) {
    uint64_t table_size = table.size();
    for (uint64_t i = 0;i < table_size; i++) {
        table.at(i) = 0;
    }
}

void OS_TRANSPARENT_MANAGEMENT::initialize_hotness_table(std::vector<HOTNESS_WIDTH>& table) {
    uint64_t table_size = table.size();
    for (uint64_t i = 0;i < table_size; i++) {
        table.at(i) = false;
    }
}
#endif // HISTORY_BASED_PAGE_SELECTION
#endif // MEMORY_USE_OS_TRANSPARENT_MANAGEMENT