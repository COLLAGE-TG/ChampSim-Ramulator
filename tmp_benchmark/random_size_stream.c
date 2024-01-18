#include <stdio.h>
#include <stdlib.h>
#include <gc.h>
#include "/home/funkytaiga/tools/gc-8.2.2/include/for_champsim.h"
#include <sys/time.h>
#include <string.h>


void random_size_stream() {
    int max_obj_size = 100000;
    int M = 40000; //繰り返し
    // 乱数生成の初期化
    srand(time(NULL));

    int **live_data = (int**)GC_MALLOC(0);
    int l_size = 0;

    for(int i=0; i < M; i++) {
        int tmp_obj_size = rand() % max_obj_size;
        int *p = (int*)GC_MALLOC_ATOMIC(sizeof(int) * tmp_obj_size);

        int tmp_r = rand() % 100;
        if(tmp_r > 90) {
            for(int p_i = 0; p_i < tmp_obj_size; p_i+=16) {
                int tmp_x_r = rand();
                p[p_i] = tmp_x_r;
            }
            l_size++;
            live_data = (int**)GC_REALLOC(live_data, l_size * sizeof(int*));
            live_data[l_size-1] = p;
        }
    }
    
    long long average = 0;
    for (int i = 0; i < l_size; i++) {
        int tmp_l_i_size = sizeof(live_data[i]) / sizeof(live_data[i][0]); // live_data[i]のサイズを取得
        // 平均を算出
        for(int p_i = 0; p_i < tmp_l_i_size; p_i+=16) {
            average += live_data[i][p_i];
        }
    }
    printf("average %d\n", average);
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

    random_size_stream();

    printf("NUM OF GC : %lu（random_size_stream.c）\n", GC_get_gc_no());
    printf("TIME OF GC : %lu ms（random_size_stream.c）\n", GC_get_full_gc_total_time());
    return 0;
}

