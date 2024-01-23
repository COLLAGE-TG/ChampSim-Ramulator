// #include "history_based_page_selection.h"
#include "os_transparent_management.h"

#if (MEMORY_USE_OS_TRANSPARENT_MANAGEMENT == ENABLE)

#if (HISTORY_BASED_PAGE_SELECTION == ENABLE)
#if (GC_MIGRATION_WITH_GC == ENABLE)
OS_TRANSPARENT_MANAGEMENT::OS_TRANSPARENT_MANAGEMENT(uint64_t max_address, uint64_t fast_memory_max_address)
: total_capacity(max_address), fast_memory_capacity(fast_memory_max_address),
  total_capacity_at_data_block_granularity(max_address >> DATA_MANAGEMENT_OFFSET_BITS),
  fast_memory_capacity_at_data_block_granularity(fast_memory_max_address >> DATA_MANAGEMENT_OFFSET_BITS),
  fast_memory_offset_bit(champsim::lg2(fast_memory_max_address)), // Note here only support integers of 2's power.
  counter_table(*(new std::vector<COUNTER_WIDTH>(max_address >> DATA_MANAGEMENT_OFFSET_BITS, COUNTER_DEFAULT_VALUE))),
  hotness_table(*(new std::vector<HOTNESS_WIDTH>(max_address >> DATA_MANAGEMENT_OFFSET_BITS, HOTNESS_DEFAULT_VALUE))),
  hotness_table_with_gc(*(new std::vector<HOTNESS_WIDTH>(max_address >> DATA_MANAGEMENT_OFFSET_BITS, HOTNESS_DEFAULT_VALUE))), // taiga added
  remapping_data_block_table(*(new std::vector<std::pair<uint64_t, bool>>(max_address >> DATA_MANAGEMENT_OFFSET_BITS)))
#else
OS_TRANSPARENT_MANAGEMENT::OS_TRANSPARENT_MANAGEMENT(uint64_t max_address, uint64_t fast_memory_max_address)
: total_capacity(max_address), fast_memory_capacity(fast_memory_max_address),
  total_capacity_at_data_block_granularity(max_address >> DATA_MANAGEMENT_OFFSET_BITS),
  fast_memory_capacity_at_data_block_granularity(fast_memory_max_address >> DATA_MANAGEMENT_OFFSET_BITS),
  fast_memory_offset_bit(champsim::lg2(fast_memory_max_address)), // Note here only support integers of 2's power.
  counter_table(*(new std::vector<COUNTER_WIDTH>(max_address >> DATA_MANAGEMENT_OFFSET_BITS, COUNTER_DEFAULT_VALUE))),
  hotness_table(*(new std::vector<HOTNESS_WIDTH>(max_address >> DATA_MANAGEMENT_OFFSET_BITS, HOTNESS_DEFAULT_VALUE))),
  remapping_data_block_table(*(new std::vector<std::pair<uint64_t, bool>>(max_address >> DATA_MANAGEMENT_OFFSET_BITS)))
#endif
{
    hotness_threshold                            = HOTNESS_THRESHOLD; //まずは1に設定しよう
    remapping_request_queue_congestion           = 0;

    // remapping_data_block_tableの初期化
    // index : physical page block address, value : hardware page block address
    for(uint64_t i=0;i < max_address >> DATA_MANAGEMENT_OFFSET_BITS; i++) {
        remapping_data_block_table.at(i).first = i;
        remapping_data_block_table.at(i).second = false;
    }
    printf("hotness threshold = %d\n", hotness_threshold);
};

OS_TRANSPARENT_MANAGEMENT::~OS_TRANSPARENT_MANAGEMENT() //良く考えてない
{
    output_statistics.remapping_request_queue_congestion = remapping_request_queue_congestion; //いらないかも

    delete &counter_table;
    delete &hotness_table;
    delete &remapping_data_block_table;
#if (GC_MIGRATION_WITH_GC == ENABLE)
    delete &hotness_table_with_gc;
#endif
};

bool OS_TRANSPARENT_MANAGEMENT::memory_activity_tracking(uint64_t address, ramulator::Request::Type type, float queue_busy_degree)
{
    if (address >= total_capacity)
    {
        std::cout << __func__ << ": phyical address input error." << std::endl;
        return false;
    }

    uint64_t p_data_block_address        = address >> DATA_MANAGEMENT_OFFSET_BITS; 

    if (DATA_MANAGEMENT_OFFSET_BITS != champsim::lg2(((4096u)))) {
        std::cout << "DATA_MANAGEMENT_GRANULARITY is not page size(4KB)\n" << std::endl;
        abort();
    }

    if (type == ramulator::Request::Type::READ) // For read request
    {
        if (counter_table.at(p_data_block_address) < COUNTER_MAX_VALUE)
        {
            counter_table[p_data_block_address]++; // Increment its counter
        }
    }
    else if (type == ramulator::Request::Type::WRITE) // For write request
    {
        if (counter_table.at(p_data_block_address) < COUNTER_MAX_VALUE)
        {
            counter_table[p_data_block_address]++; // Increment its counter
        }
    }
    else
    {
        std::cout << __func__ << ": type input error." << std::endl;
        assert(false);
    }

    // instr_count++;
    // if(instr_count >= EPOCH_LENGTH) {
    //     choose_hotpage_with_sort();
    //     add_new_remapping_request_to_queue(queue_busy_degree);
    //     instr_count = 0;
    //     clear_counter_table_epoch_count++;
    //     initialize_hotness_table(hotness_table);
    // }
    // // カウンターテーブルの初期化
    // // CLEAR_COUNTER_TABLE_EPOCH_NUMエポック毎に行う
    // if(clear_counter_table_epoch_count >= CLEAR_COUNTER_TABLE_EPOCH_NUM) {
    //     initialize_counter_table(counter_table);
    //     clear_counter_table_epoch_count = 0;
    // }

    return true;
} 

bool OS_TRANSPARENT_MANAGEMENT::choose_hotpage_with_sort() 
{
    // hotness_data_block_address_queueを初期化
    while(!hotness_data_block_address_queue.empty()) {
        hotness_data_block_address_queue.pop();
    }
    std::vector<std::pair<uint64_t, uint64_t>> tmp_pages_and_count(counter_table.size()); //this.first = （physical_data_block_address）, this.second = （counter）
    // counter_tableを複製
    for(uint64_t i =0; i < tmp_pages_and_count.size(); i++) {
        tmp_pages_and_count.at(i).first = i;
        tmp_pages_and_count.at(i).second = counter_table.at(i);
    }

    // ペアをsecond要素で降順ソート
    std::sort(tmp_pages_and_count.begin(), tmp_pages_and_count.end(), [](const auto& a, const auto& b) {
        return a.second > b.second;
    });

    // taiga debug
    std::cout << "======== hot page : access count ========" << std::endl;
    // taiga debug

    // hotness tableを更新
    for(uint64_t i = 0; i < fast_memory_capacity_at_data_block_granularity; i++) {
        // check
        if(i != fast_memory_capacity_at_data_block_granularity-1) {
            if(tmp_pages_and_count.at(i).second < tmp_pages_and_count.at(i+1).second) {
                std::cout << "ERROR:tmp_pages_and_count was not sorted." << std::endl;
                abort();
            }
        }

        // カウンターがHOTNESS_THRESHOLD 以下なら終了
        if(tmp_pages_and_count.at(i).second < HOTNESS_THRESHOLD) {
            break;
        }
        // taiga debug
        // 低速メモリにあったら
        if(remapping_data_block_table.at(tmp_pages_and_count.at(i).first).first >= fast_memory_capacity_at_data_block_granularity) {
            std::cout << tmp_pages_and_count.at(i).first << " : " << tmp_pages_and_count.at(i).second << " (in slow memory)" << std::endl;
        }
        else {
            std::cout << tmp_pages_and_count.at(i).first << " : " << tmp_pages_and_count.at(i).second << std::endl;
        }
        // taiga debug
        uint64_t tmp_hotpage_data_block_address = tmp_pages_and_count.at(i).first;
        hotness_table.at(tmp_hotpage_data_block_address) = true;
        hotness_data_block_address_queue.push(tmp_hotpage_data_block_address); //hotな順にキューに入れていく。
    }

    return true;
}

#if (GC_MIGRATION_WITH_GC == ENABLE)
#if (GC_MARKED_OBJECT == ENABLE)
bool OS_TRANSPARENT_MANAGEMENT::choose_hotpage_with_sort_with_gc(std::vector<std::uint64_t> marked_or_unmarked_pages)
{
    // hotness_data_block_address_queue_with_gcを初期化
    while(!hotness_data_block_address_queue_with_gc.empty()) {
        hotness_data_block_address_queue_with_gc.pop();
    }
    // debug
    // std::cout << "=============marked pages : count===============" << std::endl;
    for(uint64_t i = 0; i < marked_or_unmarked_pages.size(); i++) {
        uint64_t curr_p_page = marked_or_unmarked_pages.at(i);
        // check
        if(curr_p_page < 0 || curr_p_page >=total_capacity_at_data_block_granularity) {
            std::cout << "marked_pagesのアドレスが不正な値です" << std::endl;
            abort();
        }
        // taiga debug
        // std::cout << curr_p_page << " : " << counter_table.at(curr_p_page) << std::endl;
        // taiga debug
        // curr_p_pageがHOTNESSかどうか判断
        if(counter_table.at(curr_p_page) >= HOTNESS_THRESHOLD_WITH_GC) {
            // hotness tableを更新
            hotness_table_with_gc.at(curr_p_page) = true;
            hotness_data_block_address_queue_with_gc.push(curr_p_page);
        }
        
    }

    // std::cout << "=============marked pages : count (end)===============" << std::endl; // debug

    return true;
}
#else // GC_MARKED_OBJECT
bool OS_TRANSPARENT_MANAGEMENT::choose_hotpage_with_sort_with_gc_unmarked(std::vector<std::uint64_t> unmarked_pages)
{
    // hotness_data_block_address_queue_with_gcを初期化
    while(!hotness_data_block_address_queue_with_gc.empty()) {
        hotness_data_block_address_queue_with_gc.pop();
    }

    for(uint64_t i = 0; i < unmarked_pages.size(); i++) {
        uint64_t tmp_unmarked_page = unmarked_pages.at(i);
        counter_table.at(tmp_unmarked_page) = 0; // ガベージコレクションされたページはカウンターを０にする
    }

    std::vector<std::pair<uint64_t, uint64_t>> tmp_pages_and_count(counter_table.size()); //this.first = （physical_data_block_address）, this.second = （counter）
    // counter_tableを複製
    for(uint64_t i =0; i < tmp_pages_and_count.size(); i++) {
        tmp_pages_and_count.at(i).first = i;
        tmp_pages_and_count.at(i).second = counter_table.at(i);
    }

    // ペアをsecond要素で降順ソート
    std::sort(tmp_pages_and_count.begin(), tmp_pages_and_count.end(), [](const auto& a, const auto& b) {
        return a.second > b.second;
    });

    // taiga debug
    std::cout << "======== hot page : access count ========" << std::endl;
    // taiga debug

    // hotness tableを更新
    for(uint64_t i = 0; i < fast_memory_capacity_at_data_block_granularity; i++) {
        // check
        if(i != fast_memory_capacity_at_data_block_granularity-1) {
            if(tmp_pages_and_count.at(i).second < tmp_pages_and_count.at(i+1).second) {
                std::cout << "ERROR:tmp_pages_and_count was not sorted." << std::endl;
                abort();
            }
        }

        // カウンターがHOTNESS_THRESHOLD_WITH_GC以下なら終了
        if(tmp_pages_and_count.at(i).second < HOTNESS_THRESHOLD_WITH_GC) {
            // taiga debug
            std::cout << "hotpages num = " << i << std::endl;
            // taiga debug
            break;
        }
        // taiga debug
        // 低速メモリにあったら
        if(remapping_data_block_table.at(tmp_pages_and_count.at(i).first).first >= fast_memory_capacity_at_data_block_granularity) {
            std::cout << tmp_pages_and_count.at(i).first << " : " << tmp_pages_and_count.at(i).second << " (in slow memory)" << std::endl;
        }
        else {
            std::cout << tmp_pages_and_count.at(i).first << " : " << tmp_pages_and_count.at(i).second << std::endl;
        }
        // taiga debug
        uint64_t tmp_hotpage_data_block_address = tmp_pages_and_count.at(i).first;
        hotness_table_with_gc.at(tmp_hotpage_data_block_address) = true;
        hotness_data_block_address_queue_with_gc.push(tmp_hotpage_data_block_address); //hotな順にキューに入れていく。
        // debug
        // std::cout << "tmp_hotpage_data_block_address " << tmp_hotpage_data_block_address << "(choose_hotpage_with_sort_with_gc_unmarked)" << std::endl;
        // debug
    }

    return true;
}
#endif // GC_MARKED_OBJECT
#endif // GC_MIGRATION_WITH_GC
// add_new_remapping_request_to_queue内でswapのためのページを選択
// remapping_requestを返す
// 高速メモリ内のページは有効bitが0のものを選ぶ（追加機能：カウンタが0,1,2...(max=min(hotness_count))のやつを選ぶ）
// OS_TRANSPARENT_MANAGEMENT::RemappingRequest OS_TRANSPARENT_MANAGEMENT::choose_swap_page_in_fm()
// {   
//     // ================================migration ver================================
//     // hotness_data_block_address_queueのページを高速メモリに移動させるためのremapping_request発行
//     // 片方の有効bitがtrueならmigration回数は1回
//     // 両方の有効bitがtrueならmigration回数は2回
//     for(uint64_t data_block_address = 0; data_block_address < total_capacity_at_data_block_granularity; data_block_address++) {
//         // キューが空なら終了
//         if(hotness_data_block_address_queue.empty()) {
//             break;
//         }
//         // 高速メモリ内のデータで有効bitがfalseなら
//         if(remapping_data_block_table.at(data_block_address).second == false) {
//             uint64_t h_data_block_address = remapping_data_block_table.at(data_block_address).first;
//             // コールドかつ高速メモリにあるなら
//             if(h_data_block_address < fast_memory_capacity_at_data_block_granularity) { 
//                 // ホットページの中で低速メモリにあるページを選択
//                 while((hotness_data_block_address_queue.front() << DATA_MANAGEMENT_OFFSET_BITS ) < fast_memory_capacity_at_data_block_granularity) {
//                     hotness_data_block_address_queue.pop(); //高速メモリにあるホットページはpop
//                 }
//                 uint64_t hotness_data_block_address = hotness_data_block_address_queue.front();
//                 RemappingRequest remapping_request;
//                 remapping_request.address_in_fm = data_block_address << DATA_MANAGEMENT_OFFSET_BITS;
//                 remapping_request.address_in_sm = hotness_data_block_address << DATA_MANAGEMENT_OFFSET_BITS;
//             }
//         }
//     }
//     // ================================migration ver================================
// }

bool OS_TRANSPARENT_MANAGEMENT::add_new_remapping_request_to_queue(float queue_busy_degree)
{
    bool zero_migration = true;
    // ================================migration ver================================
    // hotness_data_block_address_queueのページを高速メモリに移動させるためのremapping_request発行
    // 片方の有効bitがtrueならmigration回数は1回
    // 両方の有効bitがtrueならmigration回数は2回
    
    // 有効bitがfalseのページを低速メモリへ
    for(uint64_t p_data_block_address = 0; p_data_block_address < total_capacity_at_data_block_granularity; p_data_block_address++) {
        // キューが空なら終了
        if(hotness_data_block_address_queue.empty()) {
            break;
        }
        if(hotness_table.at(p_data_block_address) == true) { //hotpageはスルー
            continue;
        }
        // 高速メモリ内のデータで有効bitがfalseなら
        if(remapping_data_block_table.at(p_data_block_address).second == false) {
            uint64_t h_data_block_address = remapping_data_block_table.at(p_data_block_address).first;
            // 高速メモリにあるなら
            if(h_data_block_address < fast_memory_capacity_at_data_block_granularity) { 
                // ホットページの中で低速メモリにあるページを選択
                while(remapping_data_block_table.at(hotness_data_block_address_queue.front()).first < fast_memory_capacity_at_data_block_granularity) {
                    hotness_data_block_address_queue.pop(); //高速メモリにあるホットページはpop
                    if(hotness_data_block_address_queue.empty()) break;
                }

                uint64_t hotness_data_block_address;
                // キューが空なら終了
                if(hotness_data_block_address_queue.empty()) {
                    break;
                }
                else {
                    hotness_data_block_address = hotness_data_block_address_queue.front();
                    hotness_data_block_address_queue.pop();
                }

                RemappingRequest remapping_request;
                remapping_request.address_in_fm = p_data_block_address << DATA_MANAGEMENT_OFFSET_BITS;
                remapping_request.address_in_sm = hotness_data_block_address << DATA_MANAGEMENT_OFFSET_BITS;
                
                // check
                if(remapping_data_block_table.at(p_data_block_address).first >= fast_memory_capacity_at_data_block_granularity) {
                    std::cout << "ERROR:there is a problem about remapping_request(fm)" << std::endl;
                    abort();
                }
                // check
                if(remapping_data_block_table.at(hotness_data_block_address).first < fast_memory_capacity_at_data_block_granularity) {
                    std::cout << "ERROR:there is a problem about remapping_request(sm)" << std::endl;
                    abort();
                }
                // check
                if(remapping_request.address_in_fm == remapping_request.address_in_sm) {
                    std::cout << "ERROR : remapping_request.address_in_fm == remapping_request.address_in_sm" << std::endl;
                    abort();
                }

                
                if (queue_busy_degree <= QUEUE_BUSY_DEGREE_THRESHOLD)
                {
                    enqueue_remapping_request(remapping_request);
                    zero_migration = false;
                }
                else {
                    std::cout << "WARNING : remapping request queue is full" << std::endl;
                    break;
                }
            }
        }
    }
    // ホットではないページを低速メモリへ
    for(uint64_t p_data_block_address = 0; p_data_block_address < total_capacity_at_data_block_granularity; p_data_block_address++) {
        // キューが空なら終了
        if(hotness_data_block_address_queue.empty()) {
            break;
        }
        if(hotness_table.at(p_data_block_address) == true) { //hotpageはスルー
            continue;
        }
        // コールドページかつ高速メモリにあるなら移動
        if(counter_table.at(p_data_block_address) < HOTNESS_THRESHOLD) {
            uint64_t h_data_block_address = remapping_data_block_table.at(p_data_block_address).first;
            // 高速メモリにあるなら
            if(h_data_block_address < fast_memory_capacity_at_data_block_granularity) { 
                // ホットページの中で低速メモリにあるページを選択
                while(remapping_data_block_table.at(hotness_data_block_address_queue.front()).first < fast_memory_capacity_at_data_block_granularity) {
                    hotness_data_block_address_queue.pop(); //高速メモリにあるホットページはpop
                    if(hotness_data_block_address_queue.empty()) break;
                }
                uint64_t hotness_data_block_address; //physical
                // キューが空なら終了
                if(hotness_data_block_address_queue.empty()) {
                    break;
                }
                else {
                    hotness_data_block_address = hotness_data_block_address_queue.front();
                    hotness_data_block_address_queue.pop();
                }

                RemappingRequest remapping_request;
                remapping_request.address_in_fm = p_data_block_address << DATA_MANAGEMENT_OFFSET_BITS;
                remapping_request.address_in_sm = hotness_data_block_address << DATA_MANAGEMENT_OFFSET_BITS;
                
                // check
                if(remapping_data_block_table.at(p_data_block_address).first >= fast_memory_capacity_at_data_block_granularity) {
                    std::cout << "ERROR:there is a problem about remapping_request(fm)" << std::endl;
                    abort();
                }
                // check
                if(remapping_data_block_table.at(hotness_data_block_address).first < fast_memory_capacity_at_data_block_granularity) {
                    std::cout << "ERROR:there is a problem about remapping_request(sm)" << std::endl;
                    abort();
                }
                // check
                if(remapping_request.address_in_fm == remapping_request.address_in_sm) {
                    std::cout << "ERROR : remapping_request.address_in_fm == remapping_request.address_in_sm" << std::endl;
                    abort();
                }

                
                if (queue_busy_degree <= QUEUE_BUSY_DEGREE_THRESHOLD)
                {
                    enqueue_remapping_request(remapping_request);
                    zero_migration = false;
                }
                else {
                    std::cout << "WARNING : remapping request queue is full" << std::endl;
                    break;
                }
            }
        }
    }
    // ================================migration ver================================
    // ================================swap ver================================
//     for(uint64_t data_block_address = 0; data_block_address < total_capacity_at_data_block_granularity; data_block_address++) {
//         // キューが空なら終了
//         if(hotness_data_block_address_queue.empty()) {
//             break;
//         }
//         // コールドページなら
//         if(hotness_table.at(data_block_address) == false) {
//             //物理アドレスからハードウェアアドレスに変換
//             uint64_t h_data_block_address = remapping_data_block_table.at(data_block_address).first;
//             if(h_data_block_address < fast_memory_capacity_at_data_block_granularity) { // コールドかつ高速メモリにあるなら
//                 uint64_t hotness_data_block_address = hotness_data_block_address_queue.front();
//                 RemappingRequest remapping_request;
//                 remapping_request.address_in_fm = data_block_address << DATA_MANAGEMENT_OFFSET_BITS;
//                 remapping_request.address_in_sm = hotness_data_block_address << DATA_MANAGEMENT_OFFSET_BITS;
//                 // check
//                 if(hotness_table.at(hotness_data_block_address) == false) {
//                     std::cout << "ERROR : hotpage is not hot" << std::endl;
//                     abort();

//                 }
//                 if(remapping_request.address_in_fm == remapping_request.address_in_sm) {
//                     std::cout << "ERROR : remapping_request.address_in_fm == remapping_request.address_in_sm" << std::endl;
//                     abort();
//                 }
//                 hotness_data_block_address_queue.pop();
//                 if (queue_busy_degree <= QUEUE_BUSY_DEGREE_THRESHOLD)
//                 {
//                     enqueue_remapping_request(remapping_request);
//                 }
//                 else {
//                     std::cout << "WARNING : remapping request queue is full" << std::endl;
//                     break;
//                 }

// #if (TEST_HISTORY_BASED_PAGE_SELECTION == ENABLE) // デバッグ用
//                 if(first_swap_epoch) {
//                     if(!remapping_request_queue.empty()) {
//                         if(first_swap) {
//                             // デバッグ結果を出力するファイルをオープン
//                             std::cout << "make check_remapping_request.txt" << std::endl;
//                             std::ofstream output_debug_file("check_remapping_request.txt", std::ios::trunc);
//                             if(output_debug_file.is_open()) {
//                                 output_debug_file << "---------------remapping request---------------" << std::endl;
//                                 output_debug_file << "remapping_request.address_in_fm data block" << " : " << "remapping_request.address_in_sm data block" << std::endl;
//                                 output_debug_file.close();
//                             }
//                             else {
//                                 std::cout << "ERROR : file cannot open" << std::endl;
//                             }
//                             first_swap = false;
//                         }
//                         else {
//                             std::ofstream output_debug_file("check_remapping_request.txt", std::ios::app);
//                             if(output_debug_file.is_open()) {
//                                 uint64_t tmp_data_block_in_fm = remapping_request.address_in_fm >> DATA_MANAGEMENT_OFFSET_BITS;
//                                 uint64_t tmp_data_block_in_sm = remapping_request.address_in_sm >> DATA_MANAGEMENT_OFFSET_BITS;
//                                 output_debug_file << tmp_data_block_in_fm << " : " << tmp_data_block_in_sm << std::endl;
//                                 output_debug_file.close();
//                             }
//                             else {
//                                 std::cout << "ERROR : debug result file cannot open" << std::endl;
//                                 abort();
//                             }
//                         }
//                     }
//                 }
// #endif // TEST_HISTORY_BASED_PAGE_SELECTION
//             }
//         }
//     }
// ================================swap ver================================
#if (TEST_HISTORY_BASED_PAGE_SELECTION == ENABLE) // デバッグ用 
// 一回記録した。 
    if(!first_swap) {
        first_swap_epoch = false;
    }
#endif

    // hotness_data_block_address_queue　初期化
    // std::queue<uint64_t> hotness_data_block_address_queue;
    while(!hotness_data_block_address_queue.empty()) {
        hotness_data_block_address_queue.pop();
    }

    // debug
    if(zero_migration) std::cout << "マイグレーションを行うべきページはありませんでした（add_new_remapping_request_to_queue）" << std::endl;
    // debug

    return true;
}

#if (GC_MIGRATION_WITH_GC == ENABLE)
bool OS_TRANSPARENT_MANAGEMENT::add_new_remapping_request_to_queue_with_gc(std::vector<std::uint64_t> marked_or_unmarked_pages)
{
    // GCを行うときに用いるadd_remapping_request
    // ================================migration ver================================
    // hotness_data_block_address_queueのページを高速メモリに移動させるためのremapping_request発行
    // 片方の有効bitがtrueならmigration回数は1回
    // 両方の有効bitがtrueならmigration回数は2回
    // 有効bitがfalseのページを低速メモリへ

    // ホットではないページを低速メモリへ
    for(uint64_t p_data_block_address = 0; p_data_block_address < total_capacity_at_data_block_granularity; p_data_block_address++) {
        // キューが空なら終了
        if(hotness_data_block_address_queue_with_gc.empty()) {
            std::cout << "at hotness_data_block_address_queue_with_gc.empty(), p_data_block_address = " << p_data_block_address << " (add_new_remapping_request_to_queue_with_gc)" <<std::endl;
            break;
        }
        // hotpageはスルー
        if(hotness_table_with_gc.at(p_data_block_address) == true) {
            continue;
        }
#if (GC_MARKED_OBJECT == DISABLE)
        // unmarked_pagesならスキップ
        bool p_data_is_unmarked = false;
        for(uint64_t i = 0; i < marked_or_unmarked_pages.size(); i++) {
            if(p_data_block_address == marked_or_unmarked_pages.at(i)) {
                p_data_is_unmarked = true;
                break;
            }
        }
        if(p_data_is_unmarked) {
            // debug
            // std::cout << "p_data_is_unmarked(add_new_remapping_request_to_queue_with_gc)" << std::endl;
            // debug
            continue;
        }
#endif // GC_MARKED_OBJECT
        // 高速メモリ内のデータで有効bitがfalseなら
        if(remapping_data_block_table.at(p_data_block_address).second == false) {
            uint64_t h_data_block_address = remapping_data_block_table.at(p_data_block_address).first;
            // 高速メモリにあるなら
            if(h_data_block_address < fast_memory_capacity_at_data_block_granularity) {
                // ホットページの中で低速メモリにあるページを選択
                while(remapping_data_block_table.at(hotness_data_block_address_queue_with_gc.front()).first < fast_memory_capacity_at_data_block_granularity) {
                    hotness_data_block_address_queue_with_gc.pop(); //高速メモリにあるページはpop
                    if(hotness_data_block_address_queue_with_gc.empty()) break;
// #if (GC_MARKED_OBJECT == DISABLE)
//                     if(remapping_data_block_table.at(hotness_data_block_address_queue_with_gc.front()).first >= fast_memory_capacity_at_data_block_granularity) {
//                         // unmarked_pagesならスキップ
//                         bool hotness_data_block_is_unmarked = false;
//                         uint64_t tmp_hotness_data_block_address = hotness_data_block_address_queue_with_gc.front(); //physical
//                         for(uint64_t i = 0; i < marked_or_unmarked_pages.size(); i++) {
//                             if(tmp_hotness_data_block_address == marked_or_unmarked_pages.at(i)) {
//                                 hotness_data_block_is_unmarked = true;
//                                 break;
//                             }
//                         }
//                         if(hotness_data_block_is_unmarked) {
//                             // debug
//                             std::cout << "hotness_data_block_is_unmarked(add_new_remapping_request_to_queue_with_gc)" << std::endl;
//                             // debug
//                             hotness_data_block_address_queue_with_gc.pop(); //unmarked pageなのでpop
//                             continue;
//                         }
//                     }
// #endif // GC_MARKED_OBJECT
                }
                uint64_t hotness_data_block_address; //physical
                // キューが空なら終了
                if(hotness_data_block_address_queue_with_gc.empty()) {
                    break;
                }
                else {
                    hotness_data_block_address = hotness_data_block_address_queue_with_gc.front();
                    hotness_data_block_address_queue_with_gc.pop();
                }

                RemappingRequest remapping_request;
                remapping_request.address_in_fm = p_data_block_address << DATA_MANAGEMENT_OFFSET_BITS;
                remapping_request.address_in_sm = hotness_data_block_address << DATA_MANAGEMENT_OFFSET_BITS;

                // check
                if(remapping_data_block_table.at(p_data_block_address).first >= fast_memory_capacity_at_data_block_granularity) {
                    std::cout << "ERROR:there is a problem about remapping_request(fm)" << std::endl;
                    abort();
                }
                // check
                if(remapping_data_block_table.at(hotness_data_block_address).first < fast_memory_capacity_at_data_block_granularity) {
                    std::cout << "ERROR:there is a problem about remapping_request(sm)" << std::endl;
                    abort();
                }
                // check
                if(remapping_request.address_in_fm == remapping_request.address_in_sm) {
                    std::cout << "ERROR : remapping_request.address_in_fm == remapping_request.address_in_sm" << std::endl;
                    abort();
                }

                // taiga debug
                // static bool first_deb_a = true;
                // if(first_deb_a) {
                //     std::cout << "=========remapping_request.address_in_fm : remapping_request.address_in_sm=========" << std::endl;
                //     first_deb_a = false;
                // }
                // std::cout << remapping_request.address_in_fm << " : " << remapping_request.address_in_sm << std::endl;
                // taiga debug
                enqueue_remapping_request(remapping_request);
            }
        }
    }
// false ページの移動
    // ホットではないページを低速メモリへ
    for(uint64_t p_data_block_address = 0; p_data_block_address < total_capacity_at_data_block_granularity; p_data_block_address++) {
        // キューが空なら終了
        if(hotness_data_block_address_queue_with_gc.empty()) {
            std::cout << "at hotness_data_block_address_queue_with_gc.empty(), p_data_block_address = " << p_data_block_address << " (add_new_remapping_request_to_queue_with_gc)" <<std::endl;
            break;
        }
        // hotpageはスルー
        if(hotness_table_with_gc.at(p_data_block_address) == true) {
            continue;
        }
#if (GC_MARKED_OBJECT == DISABLE)
        // unmarked_pagesならスキップ
        bool p_data_is_unmarked = false;
        for(uint64_t i = 0; i < marked_or_unmarked_pages.size(); i++) {
            if(p_data_block_address == marked_or_unmarked_pages.at(i)) {
                p_data_is_unmarked = true;
                break;
            }
        }
        if(p_data_is_unmarked) {
            // debug
            // std::cout << "p_data_is_unmarked(add_new_remapping_request_to_queue_with_gc)" << std::endl;
            // debug
            continue;
        }
#endif // GC_MARKED_OBJECT
        // コールドページなら
        if(counter_table.at(p_data_block_address) < HOTNESS_THRESHOLD_WITH_GC) {
            uint64_t h_data_block_address = remapping_data_block_table.at(p_data_block_address).first;
            // 高速メモリにあるなら
            if(h_data_block_address < fast_memory_capacity_at_data_block_granularity) {
                // ホットページの中で低速メモリにあるページを選択
                while(remapping_data_block_table.at(hotness_data_block_address_queue_with_gc.front()).first < fast_memory_capacity_at_data_block_granularity) {
                    hotness_data_block_address_queue_with_gc.pop(); //高速メモリにあるページはpop
                    if(hotness_data_block_address_queue_with_gc.empty()) break;
// #if (GC_MARKED_OBJECT == DISABLE)
//                     if(remapping_data_block_table.at(hotness_data_block_address_queue_with_gc.front()).first >= fast_memory_capacity_at_data_block_granularity) {
//                         // unmarked_pagesならスキップ
//                         bool hotness_data_block_is_unmarked = false;
//                         uint64_t tmp_hotness_data_block_address = hotness_data_block_address_queue_with_gc.front(); //physical
//                         for(uint64_t i = 0; i < marked_or_unmarked_pages.size(); i++) {
//                             if(tmp_hotness_data_block_address == marked_or_unmarked_pages.at(i)) {
//                                 hotness_data_block_is_unmarked = true;
//                                 break;
//                             }
//                         }
//                         if(hotness_data_block_is_unmarked) {
//                             // debug
//                             std::cout << "hotness_data_block_is_unmarked(add_new_remapping_request_to_queue_with_gc)" << std::endl;
//                             // debug
//                             hotness_data_block_address_queue_with_gc.pop(); //unmarked pageなのでpop
//                             continue;
//                         }
//                     }
// #endif // GC_MARKED_OBJECT
                }
                uint64_t hotness_data_block_address; //physical
                // キューが空なら終了
                if(hotness_data_block_address_queue_with_gc.empty()) {
                    break;
                }
                else {
                    hotness_data_block_address = hotness_data_block_address_queue_with_gc.front();
                    hotness_data_block_address_queue_with_gc.pop();
                }

                RemappingRequest remapping_request;
                remapping_request.address_in_fm = p_data_block_address << DATA_MANAGEMENT_OFFSET_BITS;
                remapping_request.address_in_sm = hotness_data_block_address << DATA_MANAGEMENT_OFFSET_BITS;

                // check
                if(remapping_data_block_table.at(p_data_block_address).first >= fast_memory_capacity_at_data_block_granularity) {
                    std::cout << "ERROR:there is a problem about remapping_request(fm)" << std::endl;
                    abort();
                }
                // check
                if(remapping_data_block_table.at(hotness_data_block_address).first < fast_memory_capacity_at_data_block_granularity) {
                    std::cout << "ERROR:there is a problem about remapping_request(sm)" << std::endl;
                    abort();
                }
                // check
                if(remapping_request.address_in_fm == remapping_request.address_in_sm) {
                    std::cout << "ERROR : remapping_request.address_in_fm == remapping_request.address_in_sm" << std::endl;
                    abort();
                }

                // taiga debug
                // static bool first_deb_a = true;
                // if(first_deb_a) {
                //     std::cout << "=========remapping_request.address_in_fm : remapping_request.address_in_sm=========" << std::endl;
                //     first_deb_a = false;
                // }
                // std::cout << remapping_request.address_in_fm << " : " << remapping_request.address_in_sm << std::endl;
                // taiga debug
                enqueue_remapping_request(remapping_request);
            }
        }
    }

    return true;
}

#endif //GC_MIGRATION_WITH_GC

void OS_TRANSPARENT_MANAGEMENT::physical_to_hardware_address(request_type& packet)
{
    uint64_t data_block_address        = packet.address >> DATA_MANAGEMENT_OFFSET_BITS;
    uint64_t remapping_data_block_address = remapping_data_block_table.at(data_block_address).first;
    remapping_data_block_table.at(data_block_address).second = true;

    packet.h_address = champsim::replace_bits(remapping_data_block_address << DATA_MANAGEMENT_OFFSET_BITS, packet.address, DATA_MANAGEMENT_OFFSET_BITS - 1);
};

void OS_TRANSPARENT_MANAGEMENT::physical_to_hardware_address(uint64_t& address)
{
    uint64_t data_block_address        = address >> DATA_MANAGEMENT_OFFSET_BITS;
    uint64_t remapping_data_block_address = remapping_data_block_table.at(data_block_address).first;
    remapping_data_block_table.at(data_block_address).second = true;
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
    // taiga debug
    // std::cout << "========= remapping address(physical) : access_conter =========" << std::endl;
    // taiga debug
    if (remapping_request_queue.empty() == false)
    {
        RemappingRequest remapping_request = remapping_request_queue.front();
        remapping_request_queue.pop_front();

        uint64_t data_block_address_in_fm        = remapping_request.address_in_fm >> DATA_MANAGEMENT_OFFSET_BITS;
        uint64_t data_block_address_in_sm        = remapping_request.address_in_sm >> DATA_MANAGEMENT_OFFSET_BITS;
        // check
        if(remapping_data_block_table.at(data_block_address_in_fm).first >= fast_memory_capacity_at_data_block_granularity) {
            std::cout << "ERROR:data_block_address_in_fm is not in fm." << std::endl;
            abort();
        }
        if(remapping_data_block_table.at(data_block_address_in_sm).first < fast_memory_capacity_at_data_block_granularity) {
            std::cout << "ERROR:data_block_address_in_sm is not in sm." << std::endl;
            std::cout << "remapping_request.address_in_fm " << remapping_request.address_in_fm << "remapping_request.address_in_sm " << remapping_request.address_in_sm << std::endl;
            std::cout << "data_block_address_in_sm " << data_block_address_in_sm << std::endl;
            std::cout << "remapping_data_block_table.at(data_block_address_in_sm).first " << remapping_data_block_table.at(data_block_address_in_sm).first << std::endl;
            exit(1);
        }
        if(data_block_address_in_fm == data_block_address_in_sm) {
            std::cout << "ERROR:data_block_address_in_fm == data_block_address_in_sm" << std::endl;
            abort();
        }

        // taiga debug
        // std::cout << "------------------------------------------------------------------------------" << std::endl;
        // std::cout << data_block_address_in_fm << ":" << counter_table.at(data_block_address_in_fm) << std::endl;
        // std::cout << data_block_address_in_sm << ":" << counter_table.at(data_block_address_in_sm) << std::endl;
        // std::cout << "------------------------------------------------------------------------------" << std::endl;
        // taiga debug

        // データswap
        uint64_t tmp_data_block_address = remapping_data_block_table.at(data_block_address_in_fm).first;
        remapping_data_block_table.at(data_block_address_in_fm).first = remapping_data_block_table.at(data_block_address_in_sm).first;
        remapping_data_block_table.at(data_block_address_in_sm).first = tmp_data_block_address;

        // valid bit変更
        bool tmp_valid_bit = remapping_data_block_table.at(data_block_address_in_fm).second;
        remapping_data_block_table.at(data_block_address_in_fm).second = remapping_data_block_table.at(data_block_address_in_sm).second;
        remapping_data_block_table.at(data_block_address_in_sm).second = tmp_valid_bit;

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


// bool OS_TRANSPARENT_MANAGEMENT::epoch_check()
// {   
//     if(instr_count == EPOCH_LENGTH) return true;
//     else if(instr_count > EPOCH_LENGTH) { // チェック
//         std::cout << "epoch_length > EPOCH_LENGTH" << std::endl; 
//         std::cout << "epoch_length = " << instr_count << std::endl;
//         abort();
//     }
//     else if(instr_count < EPOCH_LENGTH) return false;
//     else { // 通常はここには来ない
//             std::cout << "epoch_lengthの値が異常です。" << std::endl; 
//             std::cout << "epoch_length = " << instr_count << std::endl;
//             abort();
//     }
// }

// bool OS_TRANSPARENT_MANAGEMENT::remapping_request_queue_has_elements()
// {
//     if(remapping_request_queue.empty() == false) return true;
//     else if(remapping_request_queue.empty() == true) return false;
//     else {
//         std::cout << "ERROR : remapping_request_queue_has_elements()" << std::endl;
//         abort();
//     }
// }

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
    uint64_t data_block_address_in_fm        = remapping_request.address_in_fm >> DATA_MANAGEMENT_OFFSET_BITS;
    uint64_t data_block_address_in_sm        = remapping_request.address_in_sm >> DATA_MANAGEMENT_OFFSET_BITS;
    // Check duplicated remapping request in remapping_request_queue
    // If duplicated remapping requests exist, we won't add this new remapping request into the remapping_request_queue.
    bool duplicated_remapping_request  = false;
    for (uint64_t i = 0; i < remapping_request_queue.size(); i++)
    {
        uint64_t data_block_address_to_check        = remapping_request_queue[i].address_in_fm >> DATA_MANAGEMENT_OFFSET_BITS;

        if (data_block_address_in_fm == data_block_address_to_check)
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

            // check
            if(remapping_data_block_table.at(data_block_address_in_fm).first >= fast_memory_capacity_at_data_block_granularity) {
                std::cout << "ERROR:data_block_address_in_fm is not in fm(enqueue_remapping_request)." << std::endl;
                abort();
            }
            if(remapping_data_block_table.at(data_block_address_in_sm).first < fast_memory_capacity_at_data_block_granularity) {
                std::cout << "ERROR:data_block_address_in_sm is not in sm(enqueue_remapping_request)." << std::endl;
                std::cout << "data_block_address_in_sm " << data_block_address_in_sm << std::endl;
                std::cout << "remapping_data_block_table.at(data_block_address_in_sm).first " << remapping_data_block_table.at(data_block_address_in_sm).first << std::endl;
                exit(1);
            }

            // taiga debug
            
            // taiga debug

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
#if (GC_MIGRATION_WITH_GC == ENABLE)
void OS_TRANSPARENT_MANAGEMENT::initialize_hotness_table_with_gc(std::vector<HOTNESS_WIDTH>& table) {
    uint64_t table_size = table.size();
    for (uint64_t i = 0;i < table_size; i++) {
        table.at(i) = false;
    }
}

uint64_t OS_TRANSPARENT_MANAGEMENT::migration_all_start_with_gc()
{
    uint64_t migration_count_between_gc=0;
    // queueの中身全てのremappingを行う
    while(!remapping_request_queue.empty()) {
        RemappingRequest remapping_request;
        // remapping_request発行
        bool issue = issue_remapping_request(remapping_request);
        // チェック
        if (issue == false) {
            std::cout << "ERROR : The remapping request has no content." << std::endl;
            abort();
        }
        // start_swapping_segments_for_page_size(remapping_request.address_in_fm, remapping_request.address_in_sm);
        // migration_count_between_gcを更新
        // 片方の有効bitがtrueならmigration回数は1回
        // 両方の有効bitがtrueならmigration回数は2回
        uint64_t data_address_in_fm = remapping_request.address_in_fm >> DATA_MANAGEMENT_OFFSET_BITS;
        uint64_t data_address_in_sm = remapping_request.address_in_sm >> DATA_MANAGEMENT_OFFSET_BITS;
        if(remapping_data_block_table.at(data_address_in_fm).second == true && remapping_data_block_table.at(data_address_in_sm).second == true) {
            migration_count_between_gc += 2;
            // check
            // if(active_entry_number != 1) {
            //     std::cout << "ERROR:wrong active_entry_number" << std::endl;
            // }
            // active_entry_number += 1;
        }
        else if(remapping_data_block_table.at(data_address_in_fm).second == false && remapping_data_block_table.at(data_address_in_sm).second == false){ //errorチェック
            std::cout << "ERROR:remapping_request is something wrong" << std::endl;
            exit(1);
        }
        else {
            migration_count_between_gc++;
        }

        // remapping_requestからデータをポップして、remapping_data_block_tableの書き換え
        finish_remapping_request();
        
        // initialize_swapping();
    }

    // sum_migration_with_gc_countはgc中にmigrationした回数の合計
    sum_migration_with_gc_count += migration_count_between_gc;

    // migrationにかかったオーバーヘッドを計算
    // uint64_t migration_cycle_between_gc = OVERHEAD_OF_MIGRATION_PER_PAGE * migration_count_between_gc;
    // migration_cycle_between_gc += OVERHEAD_OF_CHANGE_PTE_PER_PAGE * migration_count_between_gc;
    // TLBのオーバーヘッドはGCと同時にはできない。
    // migration_cycle_between_gc += OVERHEAD_OF_TLB_SHOOTDOWN_PER_PAGE * migration_count_between_gc;

    return migration_count_between_gc;
}
#endif // GC_MIGRATION_WITH_GC
#endif // HISTORY_BASED_PAGE_SELECTION
#endif // MEMORY_USE_OS_TRANSPARENT_MANAGEMENT