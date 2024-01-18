#include <stdio.h>
#include <stdlib.h>
#include <gc.h>
#include "/home/funkytaiga/tools/gc-8.2.2/include/for_champsim.h"
#include <sys/time.h>
#include <string.h>

void seq() {
    // 大きい配列を静的に確保
    long long array_size = 1000000;
    long long array[array_size];
    for(long long i = 0; i < array_size; i++) {
        array[i] = 0;
    }

    long long cur_array_i = 0; // arrayのインデックス
    int max_obj_size = 100000; // オブジェクトサイズの最大値
    int M = 70000; //繰り返し
    // 乱数生成の初期化
    srand(time(NULL));

    // 前のオブジェクトを保持
    long long* pre_obj;
    long long pre_obj_size;

    for(long long i = 0; i < M; i++) {
        long long tmp_obj_size = rand() % max_obj_size;
        long long *tmp_obj = (long long *)GC_MALLOC_ATOMIC(sizeof(long long) * tmp_obj_size);
        // 1回目は行わない
        if(i != 0) {
            // pre_objの値をarrayに格納
            for(long long j = cur_array_i; j < pre_obj_size; j += 8) {
                long long pre_obj_ind = j - cur_array_i;
                // pre_objのサイズを超えたら終了
                if(pre_obj_ind >= pre_obj_size) break;

                array[j] = pre_obj[pre_obj_ind];
                cur_array_i += 8;
                // arrayの最後まで達したら0から再スタート
                if(cur_array_i >= array_size) {
                    cur_array_i = 0;
                }
            }
        }

        // tmp_objの各要素に値を入力
        for(long long j = 0; j < tmp_obj_size; j += 8) {
            // tmp_objのサイズを超えたら終了
            if(j >= tmp_obj_size) break;
            tmp_obj[j] = 1;
        }
        pre_obj = tmp_obj;
        pre_obj_size = tmp_obj_size;
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
    printf("start（random_size_stream.c）\n");

    seq();

    printf("NUM OF GC : %lu（seq_benchmark.c）\n", GC_get_gc_no());
    printf("TIME OF GC : %lu ms（seq_benchmark.c）\n", GC_get_full_gc_total_time());
    return 0;
}

