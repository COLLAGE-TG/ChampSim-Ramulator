#include <stdio.h>
#include <stdlib.h>
#include <gc/gc.h>
#include <sys/time.h>

int main(void)
{
  GC_INIT();
  GC_enable_incremental(); //小さいGCを多く行う。世代別とはちがう？
  GC_start_performance_measurement();
  printf("start\n");
  long long Np = 100;
  long long Nc = 1000000;
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
    for (long long p = 0; p < Np; p++)
    {
      for (long long q = 0; q < Nc; q = q + 8) //cache lineの大きさが64B、ll intの大きさが8Bなので８ずつアクセスすれば配列の持つ全てのメモリ空間にアクセスできる。
      {
        if (q >= Nc) break;
        long long res = (ph[p] + ch[q]) / 2;
      }
    }

    // 結果を出力
    // for(ll k=0; k<N; k++){
    //   printf("%d ",ave[k]);
    // }
    // printf("\n\n");
    // printf("------------------------------------------------------------------------------------------------------------------------------------------------------\n\n");

    printf("%d回目\n", i);

    // メモリ解放
    // free(ph);
    // free(ch);
  }

  printf("NUM OF GC : %lu\n", GC_get_gc_no());
  printf("TIME OF GC : %lu ms\n", GC_get_full_gc_total_time());
  return 0;
}