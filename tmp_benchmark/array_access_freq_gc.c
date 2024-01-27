// GCが起きる頻度を調節できるようにする
#include <stdio.h>
#include <stdlib.h>
#include <gc.h>
#include "/home/funkytaiga/tools/gc-8.2.2/include/for_champsim.h"
#include <sys/time.h>
#include <string.h>

#define CACHE_SIZE (64)
#define PAGE_SIZE (4096)

typedef long long ll;


void array_access() {
    printf("===hello array===\n");
    int num_ele_in_page = 512;
    int num_ele_in_cache = 8;
    ll min_array_pages = 100;
    ll min_array_num_ele = num_ele_in_page * min_array_pages;
    ll min_obj_pages = 400;
    ll min_obj_size = num_ele_in_page * min_obj_pages;
    // ll ave_obj_size = (min_obj_size/2) + min_obj_size;
    int max_access_one_ws = 10000000; // 一つのワーキングセットに何回アクセスするか
    int max_num_make_obj = 500000; // 繰り返し
    int max_num_change_ws = 10; // 繰り返し
    int max_access_one_obj = max_access_one_ws / max_num_make_obj; // 一つのオブジェクトに何回アクセスするのか
    // printf("===middle array===\n");
    // check
    // if(max_access_one_obj > min_array_num_ele / num_ele_in_cache) {
    //     printf("max_access_one_obj >= min_array_num_ele\n");
    //     exit(1);
    // }
    // if(max_access_one_obj >= min_obj_size) {
    //     printf("max_access_one_obj >= min_obj_size\n");
    //     exit(1);
    // }
    if(max_access_one_obj >= min_obj_size / num_ele_in_cache) {
        printf("max_access_one_obj >= min_obj_size / num_ele_in_cach\n");
        exit(1);
    }

    printf("--------------\n");

    for(int m = 0; m < max_num_change_ws; m++) {
        ll array_size = (rand() % (min_array_num_ele)) + (min_array_num_ele); // 100ページから200ページのarray
        ll ind_array = 0; // arrayのインデックス
        ll *array = (ll *)GC_MALLOC_ATOMIC(sizeof(ll) * array_size);
        for(ll n = 0; n < max_num_make_obj; n++) {
            ll obj_size = (rand() % min_obj_size) + min_obj_size;
            ll *obj = (ll*)GC_MALLOC_ATOMIC(sizeof(ll) * obj_size);
            for(ll ind_obj = 0; ind_obj < max_access_one_obj; ind_obj += 8) {
                array[ind_array] = obj[ind_obj];
                // ind_arrayの更新
                ind_array += 8;
                if(ind_array >= array_size) ind_array = 0;
                if(ind_obj >= obj_size) ind_obj = 0;
            }
        }

    }
}

int main(void)
{
    FILE *file = fopen(marked_bit_file_path, "w");
    printf("marked_bit_file_path = %s\n", marked_bit_file_path);
    if (file == NULL) {
        fprintf(stderr, "ファイル %s を開けませんでした(test1.c)\n", marked_bit_file_path);
        return 1;
    }
    fclose(file);
    
    GC_INIT();
    GC_enable_incremental();
    GC_start_performance_measurement();

    // 大きい配列を静的に確保
    long long dummy_array_size = 1000000;
    long long dummy_array[dummy_array_size];
    // 1ページ毎アクセス
    for(long long i = 0; i < dummy_array_size; i += 512) {
        dummy_array[i] = 0;
    }

    array_access();

    printf("NUM OF GC : %lu（array_access.c）\n", GC_get_gc_no());
    printf("TIME OF GC : %lu ms（array_access.c）\n", GC_get_full_gc_total_time());
    return 0;
}

