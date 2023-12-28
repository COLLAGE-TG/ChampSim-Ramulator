#include <stdio.h>
#include <stdlib.h>
#include <gc.h>
#include "/home/funkytaiga/tools/gc-8.2.2/include/for_champsim.h"
#include <sys/time.h>
#include <string.h>


int main(void)
{
  // if (argc != 4) {
  //   fprintf(stderr, "引数の数が適当ではありません。\n argc = %d\n",argc);
  //   return 1;
  // }
  //   char program_name[MAX_PATH_LENGTH];
  //   // char program_name_no_dot[MAX_PATH_LENGTH];
  //   char warmup_instruction[MAX_PATH_LENGTH];
  //   char simulation_instruction[MAX_PATH_LENGTH];
  //   char marked_bit_file_path[MAX_PATH_LENGTH];
  //   char path_to_dir[] = "/home/funkytaiga/tmp_champ/ChampSim-Ramulator/tmp_gc_marked_pages_files/";

  //   memset(program_name, 0, sizeof(program_name));
  //   // memset(program_name_no_dot, 0, sizeof(program_name_no_dot));
  //   memset(warmup_instruction, 0, sizeof(warmup_instruction));
  //   memset(simulation_instruction, 0, sizeof(simulation_instruction));
  //   memset(marked_bit_file_path, 0, sizeof(marked_bit_file_path));

  //   strcpy(program_name, argv[1]);
  //   strcpy(warmup_instruction, argv[2]);
  //   strcpy(simulation_instruction, argv[3]);

  //   // char *dot = strrchr(program_name, '.');
  //   // if (dot != NULL) {
  //   //     // ".txt" を取り除く
  //   //     *dot = '\0';
  //   // }

  //   strcpy(marked_bit_file_path, path_to_dir);
  //   strcat(marked_bit_file_path, program_name);
  //   strcat(marked_bit_file_path, "_");
  //   strcat(marked_bit_file_path, warmup_instruction);
  //   strcat(marked_bit_file_path, "_");
  //   strcat(marked_bit_file_path, simulation_instruction);
  //   strcat(marked_bit_file_path, ".txt");

  FILE *file = fopen(marked_bit_file_path, "w");
  printf("marked_bit_file_path = %s\n", marked_bit_file_path);
  if (file == NULL) {
    fprintf(stderr, "ファイル %s を開けませんでした(test1.c)\n", marked_bit_file_path);
    return 1;
  }
  // fprintf(file, "%s\n", program_name);
  // fprintf(file, "warm up %s\nsimulation %s\n", warmup_instruction, simulation_instruction);
  fclose(file);
  
  GC_INIT();
  GC_enable_incremental(); //小さいGCを多く行う。世代別とはちがう？
  GC_start_performance_measurement();
  printf("start（test1.c）\n");
  long long Np = 100;
  long long Nc = 10000;
  long long M = 100; //繰り返し

  for (long long i = 0;i < M;i++)
  {
    long long* ph;
    long long* ch;

    ph = (long long*)GC_MALLOC(sizeof(long long) * Np);
    ch = (long long*)GC_MALLOC(sizeof(long long) * Nc);
    if (ph == NULL) exit(0);
    if (ch == NULL) exit(0);
    // 100点満点で点数を入力
    for (long long k = 0; k < Np; k++)
    {
      ph[k] = rand() % 101;
    }
    for (long long k = 0; k < Nc; k++)
    {
      ch[k] = rand() % 101;
    }

    // 平均を出す。
    // for (long long p = 0; p < Np; p++)
    // {
    //   for (long long q = 0; q < Nc; q = q + 8) //cache lineの大きさが64B、ll intの大きさが8Bなので８ずつアクセスすれば配列の持つ全てのメモリ空間にアクセスできる。
    //   {
    //     if (q >= Nc) break;
    //     long long res = (ph[p] + ch[q]) / 2;
    //   }
    // }

    // 結果を出力
    // for(ll k=0; k<N; k++){
    //   printf("%d ",ave[k]);
    // }
    // printf("\n\n");
    // printf("------------------------------------------------------------------------------------------------------------------------------------------------------\n\n");

    printf("%d回目（test1.c）\n", i);

    // メモリ解放
    // free(ph);
    // free(ch);
  }

  printf("NUM OF GC : %lu（test1.c）\n", GC_get_gc_no());
  printf("TIME OF GC : %lu ms（test1.c）\n", GC_get_full_gc_total_time());
  return 0;
}