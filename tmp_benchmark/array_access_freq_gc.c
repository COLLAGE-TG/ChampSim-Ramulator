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
    int num_ele_in_page = 512;
    int num_ele_in_cache = 8;

    ll max_num_make_obj = 150; // 一つのワーキングセットで何回オブジェクトを生成するか（変数）(50~100)
    ll min_array_pages = 100;
    ll max_access_one_ws = 100000; // 一つのワーキングセットに何回アクセスするか
    ll max_num_change_ws = 10; // 何個のワーキングセットを使うか
    ll min_obj_pages = 200; // オブジェクトの最小ページ数
    ll plus_page_to_array = 10; // ワーキングセットは何ページずつ増えるのか
    ll max_pages_in_obj = 800; // objサイズの限界

    ll plus_page_to_obj = (2 * (max_access_one_ws - (max_num_make_obj * min_obj_pages))) / (max_num_make_obj * (max_num_make_obj + 1));
    ll min_array_num_ele = num_ele_in_page * min_array_pages;
    ll min_obj_num_ele = num_ele_in_page * min_obj_pages;
    ll num_cachelines_in_obj = max_access_one_ws / max_num_make_obj; // 一つのオブジェクトに何回アクセスするのか
    ll min_num_ele_in_obj = num_cachelines_in_obj * num_ele_in_cache;

    // check
    if(plus_page_to_obj <= 0) {
        printf("plus_page_to_obj <= 0\n");
        exit(1);
    }
    // if(max_cachelines_in_array * 2 < num_cachelines_in_obj) {
    //     printf("max_cachelines_in_array < num_cachelines_in_obj\n");
    //     exit(1);
    // }

    ll array_size = min_array_num_ele;
    ll obj_size = min_num_ele_in_obj;

    for(int m = 0; m < max_num_change_ws; m++) { // ワーキングセットの切り替え
        // ll array_size = (rand() % (min_array_num_ele)) + (min_array_num_ele); // 100ページから200ページのarray
        ll i_array = 0; // arrayのインデックス
        ll *array = (ll *)GC_MALLOC_ATOMIC(sizeof(ll) * array_size);
        for(ll n = 0; n < max_num_make_obj; n++) {  // オブジェクトの切り替え
            ll *obj = (ll*)GC_MALLOC_ATOMIC(sizeof(ll) * obj_size);
            for(ll i_obj = 0; i_obj < obj_size; i_obj += 8) {
                array[i_array] = obj[i_obj];
                // i_arrayの更新
                i_array += 8;
                if(i_array >= array_size) i_array = 0;
            }
            // objサイズが大きくなりすぎたら初期化
            if(obj_size / num_ele_in_page >= max_pages_in_obj) {
                ll tmp_rest_pages = ( obj_size / num_ele_in_page ) - max_pages_in_obj;
                obj_size = min_num_ele_in_obj + (tmp_rest_pages * num_ele_in_page);
            }
            obj_size += (plus_page_to_obj * num_ele_in_page);
        }
        array_size += (plus_page_to_array * num_ele_in_page);
    }

    printf("==stats==\n");
    // printf("max_cachelines_in_array : %lu\n", max_cachelines_in_array);
    printf("max_access_one_ws : %lu\n", max_access_one_ws);
    printf("max_num_make_obj : %lu\n", max_num_make_obj);
    printf("max_num_change_ws : %lu\n", max_num_change_ws);
    printf("num_cachelines_in_obj : %lu\n", num_cachelines_in_obj);
    printf("plus_page_to_obj : %lu\n", plus_page_to_obj);
    printf("array_size : %lu\n", array_size);

    // printf("num_ele_in_obj : %lu\n", num_ele_in_obj);
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

