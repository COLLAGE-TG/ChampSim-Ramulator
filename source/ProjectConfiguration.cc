#include "ProjectConfiguration.h"

MEMORY_TRACE output_memorytrace("memory trace", ".trace");
SIMULATOR_STATISTICS output_statistics("ChampSim statistics", ".statistics");

DATA_OUTPUT::DATA_OUTPUT(std::string v1, std::string v2)
: data_name(v1), file_extension(v2)
{
}

DATA_OUTPUT::~DATA_OUTPUT()
{
    if (file_handler) // check the validity of this file handler
    {
        fclose(file_handler);
    }

    if (file_name) // check the validity of this file name string
    {
        printf("Output %s into %s\n", data_name.c_str(), file_name);
        free(file_name);
    }
}

void DATA_OUTPUT::output_file_initialization(char** string_array, uint32_t number, uint64_t warmup_instr, uint64_t simulation_instr)
{
    std::string benchmark_names;
    for (uint32_t i = 0; i < number; i++)
    {
        char* string_temp = (char*) malloc(strlen(string_array[i]) + 1);
        strcpy(string_temp, string_array[i]);

        const char* delimiter = "/";
        char empty[]          = "";
        char* token           = empty;
        char* last_token      = empty;

        /* get the first token */
        token                 = strtok(string_temp, delimiter);

        /* walk through other tokens */
        while (token != NULL)
        {
            last_token = token;
            token      = strtok(NULL, delimiter);
        }

        benchmark_names += std::string(last_token) + "_";

        free(string_temp);
    }
    benchmark_names.erase(benchmark_names.size() - 1);

    // warmup,simulation instructionを付与
    benchmark_names += "_cw_";
    benchmark_names += std::to_string(warmup_instr);
    benchmark_names += "_cs_";
    benchmark_names += std::to_string(simulation_instr);

    // taiga added
#if (MEMORY_USE_OS_TRANSPARENT_MANAGEMENT==ENABLE)
#if (GC_MARKED_OBJECT == ENABLE)
    benchmark_names += "marked_";
#else
    benchmark_names += "_unmarked";
#endif

#if(NO_METHOD_FOR_RUN_HYBRID_MEMORY == ENABLE)
    benchmark_names += "_no_method_migration";
#endif // NO_METHOD_FOR_RUN_HYBRID_MEMORY

#if (NO_MIGRATION == ENABLE)
    benchmark_names += "_nomigration";
#else // NO_MIGRATION

#if (GC_MIGRATION_WITH_GC == ENABLE)
    benchmark_names += "_gcmigration";
#endif

#if (HISTORY_BASED_PAGE_SELECTION == ENABLE)
    benchmark_names += "_epoch_";
    benchmark_names += std::to_string(EPOCH_LENGTH);
    benchmark_names += "_cleartable_";
    benchmark_names += std::to_string(CLEAR_COUNTER_TABLE_EPOCH_NUM);
#endif

#endif // NO_MIGRATION
#endif // MEMORY_USE_OS_TRANSPARENT_MANAGEMENT
    // taiga added

    // append file_extension to benchmark_names.
    // benchmark_names += file_extension.c_str();
    benchmark_names += file_extension;

    // taiga added
    std::string tmp_output_file_path_statistic = output_file_path_statistic;
    tmp_output_file_path_statistic += benchmark_names.c_str();
    benchmark_names = tmp_output_file_path_statistic;
    // taiga added

    file_handler = fopen(benchmark_names.c_str(), "w");
    file_name    = (char*) malloc(benchmark_names.size() + 1);
    strcpy(file_name, benchmark_names.c_str());
}

MEMORY_TRACE::MEMORY_TRACE(std::string v1, std::string v2)
: DATA_OUTPUT(v1, v2)
{
}

MEMORY_TRACE::MEMORY_TRACE(std::string v1, std::string v2, char** string_array, uint32_t number, uint64_t warmup_instr, uint64_t simulation_instr)
: DATA_OUTPUT(v1, v2)
{
    output_file_initialization(string_array, number, warmup_instr, simulation_instr);
}

MEMORY_TRACE::~MEMORY_TRACE()
{
    if (file_handler) // check the validity of this file handler
    {
    }
}

void MEMORY_TRACE::output_memory_trace_hexadecimal(uint64_t address, char type)
{
    assert(file_handler);
    fprintf(file_handler, "0x%lx %c\n", address, type);
}

SIMULATOR_STATISTICS::SIMULATOR_STATISTICS(std::string v1, std::string v2)
: DATA_OUTPUT(v1, v2)
{
    statistics_initialization();
}

SIMULATOR_STATISTICS::SIMULATOR_STATISTICS(std::string v1, std::string v2, char** string_array, uint32_t number, uint64_t warmup_instr, uint64_t simulation_instr)
: DATA_OUTPUT(v1, v2)
{
    output_file_initialization(string_array, number, warmup_instr, simulation_instr);
    statistics_initialization();
}

SIMULATOR_STATISTICS::~SIMULATOR_STATISTICS()
{
    if (file_handler) // check the validity of this file handler
    {
        fprintf(file_handler, "\n\nInformation about virtual memory\n\n");
        for (uint64_t i = 0; i < valid_pte_count.size(); i++)
        {
            fprintf(file_handler, "Level: %ld, valid_pte_count: %ld.\n", i, valid_pte_count[i]);
        }
        fprintf(file_handler, "virtual_page_count: %ld, main memory footprint: %f MB.\n", virtual_page_count, virtual_page_count * 4.0 / KiB);

        uint64_t total_access_request_in_memory = read_request_in_memory + read_request_in_memory2 + write_request_in_memory + write_request_in_memory2;

        fprintf(file_handler, "\n\nInformation about memory controller\n\n");
        fprintf(file_handler, "read_request_in_memory: %ld, read_request_in_memory2: %ld.\n", read_request_in_memory, read_request_in_memory2);
        fprintf(file_handler, "write_request_in_memory: %ld, write_request_in_memory2: %ld.\n", write_request_in_memory, write_request_in_memory2);
        fprintf(file_handler, "hit rate: %f.\n", (read_request_in_memory + write_request_in_memory) / float(total_access_request_in_memory));
        fprintf(file_handler, "read_request_in_memory: %ld, read_request_in_memory2: %ld.\n", migration_read_request_in_memory, migration_read_request_in_memory2);
        fprintf(file_handler, "write_request_in_memory: %ld, write_request_in_memory2: %ld.\n", migration_write_request_in_memory, migration_write_request_in_memory2);
        fprintf(file_handler, "read_request_in_memory: %ld, read_request_in_memory2: %ld.\n", gcmigration_read_request_in_memory, gcmigration_read_request_in_memory2);
        fprintf(file_handler, "write_request_in_memory: %ld, write_request_in_memory2: %ld.\n", gcmigration_write_request_in_memory, gcmigration_write_request_in_memory2);
#if (TRACKING_LOAD_STORE_STATISTICS == ENABLE)
        fprintf(file_handler, "load_request_in_memory: %ld, load_request_in_memory2: %ld.\n", load_request_in_memory, load_request_in_memory2);
        fprintf(file_handler, "store_request_in_memory: %ld, store_request_in_memory2: %ld.\n", store_request_in_memory, store_request_in_memory2);
#endif // TRACKING_LOAD_STORE_STATISTICS
#if (HISTORY_BASED_PAGE_SELECTION == ENABLE)
        migration_cycles = (OVERHEAD_OF_MIGRATION_PER_PAGE + OVERHEAD_OF_TLB_SHOOTDOWN_PER_PAGE) * swapping_count;
        fprintf(file_handler, "migration_count: %ld, migration_traffic_in_bytes: %ld migration_cycles %ld.\n", swapping_count, swapping_traffic_in_bytes, migration_cycles);
#else
        fprintf(file_handler, "swapping_count: %ld, swapping_traffic_in_bytes: %ld.\n", swapping_count, swapping_traffic_in_bytes);
#endif
#if (GC_MIGRATION_WITH_GC == ENABLE)
        migration_cycles_with_gc = (OVERHEAD_OF_MIGRATION_PER_PAGE + OVERHEAD_OF_TLB_SHOOTDOWN_PER_PAGE) * sum_migration_with_gc_count;
        fprintf(file_handler, "migration_with_gc_count: %ld migration_cycles_with_gc %ld.\n", sum_migration_with_gc_count, migration_cycles_with_gc);  
        // debug
        fprintf(file_handler, "gcmigration_tlb_overhead  %ld, gcmigration_sum_overhead_without_tlb %ld\n", gcmigration_tlb_overhead, gcmigration_sum_overhead_without_tlb);
        // debug   
#endif // GC_MIGRATION_WITH_GC

#if (HISTORY_BASED_PAGE_SELECTION == ENABLE)
        fprintf(file_handler, "HOTNESS_THRESHOLD: %u.\n", HOTNESS_THRESHOLD);
#endif
#if (GC_MIGRATION_WITH_GC == ENABLE)
        fprintf(file_handler, "HOTNESS_THRESHOLD_WITH_GC: %u.\n", HOTNESS_THRESHOLD_WITH_GC);        
#endif // GC_MIGRATION_WITH_GC

        fprintf(file_handler, "remapping_request_queue_congestion: %ld.\n", remapping_request_queue_congestion);

#if (IDEAL_VARIABLE_GRANULARITY == ENABLE)
        fprintf(file_handler, "no_free_space_for_migration: %ld (%f).\n", no_free_space_for_migration, no_free_space_for_migration / float(total_access_request_in_memory));
        fprintf(file_handler, "no_invalid_group_for_migration: %ld (%f).\n", no_invalid_group_for_migration, no_invalid_group_for_migration / float(total_access_request_in_memory));
        fprintf(file_handler, "unexpandable_since_start_address: %ld (%f).\n", unexpandable_since_start_address, unexpandable_since_start_address / float(total_access_request_in_memory));
        fprintf(file_handler, "unexpandable_since_no_invalid_group: %ld (%f).\n", unexpandable_since_no_invalid_group, unexpandable_since_no_invalid_group / float(total_access_request_in_memory));
        fprintf(file_handler, "data_eviction_success: %ld (%f).\n", data_eviction_success, data_eviction_success / float(total_access_request_in_memory));
        fprintf(file_handler, "data_eviction_failure: %ld (%f).\n", data_eviction_failure, data_eviction_failure / float(total_access_request_in_memory));
        fprintf(file_handler, "uncertain_counter: %ld (%f).\n", uncertain_counter, uncertain_counter / float(total_access_request_in_memory));
#endif // IDEAL_VARIABLE_GRANULARITY
    }
}

void SIMULATOR_STATISTICS::statistics_initialization()
{
    /* Initialize variabes */
    for (uint64_t i = 0; i < valid_pte_count.size(); i++)
    {
        valid_pte_count[i] = 0;
    }
    virtual_page_count = 0;

#if (TRACKING_LOAD_STORE_STATISTICS == ENABLE)
    load_request_in_memory   = 0;
    load_request_in_memory2  = 0;
    store_request_in_memory  = 0;
    store_request_in_memory2 = 0;
#endif // TRACKING_LOAD_STORE_STATISTICS

    read_request_in_memory             = 0;
    read_request_in_memory2            = 0;
    write_request_in_memory            = 0;
    write_request_in_memory2           = 0;

    swapping_count                     = 0;
    swapping_traffic_in_bytes          = 0;

    remapping_request_queue_congestion = 0;

#if (IDEAL_VARIABLE_GRANULARITY == ENABLE)
    no_free_space_for_migration         = 0;
    no_invalid_group_for_migration      = 0;
    unexpandable_since_start_address    = 0;
    unexpandable_since_no_invalid_group = 0;
    data_eviction_success = data_eviction_failure = 0;
    uncertain_counter                             = 0;
#endif // IDEAL_VARIABLE_GRANULARITY
}
