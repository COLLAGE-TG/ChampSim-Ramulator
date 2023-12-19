#include <stdio.h>
#include <stdlib.h>
#include <gc/gc.h>
#include "/home/funkytaiga/tools/gc-8.2.2/include/for_champsim.h"
#include <sys/time.h>

int main(void)
{
  // fileを初期化
  // char *main_file_name = __FILE__;
  // concatStrings(main_file_name, ".txt");
  // char *tmp_marked_bit_file_path = "/home/funkytaiga/tmp_champ/ChampSim-Ramulator/tmp_gc_marked_pages_files/";
  // concatStrings(tmp_marked_bit_file_path, main_file_name);
  FILE *file = fopen(marked_bit_file_path, "w");
  printf("marked_bit_file_path = %s\n", marked_bit_file_path);
  if (file == NULL) {
    fprintf(stderr, "ファイル %s を開けませんでした(test2.c)\n", marked_bit_file_path);
    return 1;
  }
  fclose(file);

  GC_INIT();
  GC_enable_incremental(); //小さいGCを多く行う。世代別とはちがう？
  GC_start_performance_measurement();
  printf("start（test2.c）\n");
  long long Np = 100;
  long long Nc = 1000000;
  long long dif = 1000;
  long long M = 100; //繰り返し

  for (long long i = 0;i < M;i++)
  {
    long long* ph;
    long long* ch;

    long long tmp_Np = Np + i * dif;
    long long tmp_Nc = Nc + i * dif;

    ph = (long long*)GC_MALLOC(sizeof(long long) * tmp_Np);
    ch = (long long*)GC_MALLOC(sizeof(long long) * tmp_Nc);
    if (ph == NULL) exit(0);
    if (ch == NULL) exit(0);
    // 100点満点で点数を入力
    for (long long k = 0; k < tmp_Np; k++)
    {
      ph[k] = rand() % 101;
    }
    for (long long k = 0; k < tmp_Nc; k++)
    {
      ch[k] = rand() % 101;
    }

    // 平均を出す。
    for (long long p = 0; p < tmp_Np; p++)
    {
      for (long long q = 0; q < tmp_Nc; q = q + 8) //cache lineの大きさが64B、ll intの大きさが8Bなので８ずつアクセスすれば配列の持つ全てのメモリ空間にアクセスできる。
      {
        if (q >= tmp_Nc) break;
        long long res = (ph[p] + ch[q]) / 2;
      }
    }

    // 結果を出力
    // for(ll k=0; k<N; k++){
    //   printf("%d ",ave[k]);
    // }
    // printf("\n\n");
    // printf("------------------------------------------------------------------------------------------------------------------------------------------------------\n\n");

    printf("%d回目（test2.c）\n", i);

    // メモリ解放
    // free(ph);
    // free(ch);
  }

  printf("NUM OF GC : %lu（test2.c）\n", GC_get_gc_no());
  printf("TIME OF GC : %lu ms（test2.c）\n", GC_get_full_gc_total_time());
  return 0;
}