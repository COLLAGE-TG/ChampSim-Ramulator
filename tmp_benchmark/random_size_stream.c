#include <stdio.h>
#include <stdlib.h>
#include <gc.h>
#include "/home/funkytaiga/tools/gc-8.2.2/include/for_champsim.h"
#include <sys/time.h>
#include <string.h>

// 引数としてwarmup_instructionとsimulation_instruction, "(un)marked"を渡す。

void random_size_stream() {
    int max_obj_size = 10000;
    int M = 10000; //繰り返し
    // 乱数生成の初期化
    srand(time(NULL));

    int **live_data = (int**)GC_MALLOC(0);
    int l_size = 0;

    for(int i=0; i < M; i++) {
        int tmp_obj_size = rand() % max_obj_size;
        int *p = (int*)GC_MALLOC_ATOMIC(sizeof(int) * tmp_obj_size);

        int tmp_r = rand() % 100;
        if(tmp_r > 90) {
            for(int p_i = 0; p_i < tmp_obj_size; p_i++) {
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
        for(int p_i = 0; p_i < tmp_l_i_size; p_i++) {
            average += live_data[i][p_i];
        }
    }
    printf("average %d\n", average);
}

void make_filename(char* ex_filename,char* str_warmup_instr,char* str_simulation_instr, char* is_marked) {
    char tmp_filename[256];
    char* delimiter = ".";
    char* token = strtok(ex_filename, delimiter);
    // char str_warmup_instr[16], str_simulation_instr[16];
    char output_filename[512];

    if(token != NULL) {
        strcpy(tmp_filename, token);
    }
    else {
        fprintf(stderr, "実行ファイルが.outで終わっていません。\n");
        exit(1);
    }

    // sprintf(str_warmup_instr, "%d", warmup_instr);
    // sprintf(str_simulation_instr, "%d", simulation_instr);

    strcpy(output_filename, tmp_filename);
    strcat(output_filename, "_");
    strcat(output_filename, str_warmup_instr);
    strcat(output_filename, "_");
    strcat(output_filename, str_simulation_instr);
    strcat(output_filename, "_");
    strcat(output_filename, is_marked);
    strcat(output_filename, ".txt");

    int size_of_o_file = sizeof(output_filename) / sizeof(output_filename[0]);
    init_markpath(size_of_o_file);

    for(int i=0;i < size_of_o_file; i++){
        marked_bit_file_path[i] = output_filename[i];
    }
}


int main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "引数は4つ必要です。\n");
        return 1;
    }

    make_filename(argv[0], argv[1], argv[2], argv[3]);

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

