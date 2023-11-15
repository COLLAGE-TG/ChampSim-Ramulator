#include <stdio.h>
#include <stdlib.h>
#include <gc_cpp.h> 

using namespace std;
using ll = long long;

const ll Np = 100;
const ll Nc = 1000000;
const ll M = 10; //繰り返し

class A : public gc_cleanup
{
  ll a_ll;
};

int main(void)
{
  GC_INIT();
  for (ll i = 0;i < M;i++)
  {
    A* physics = new (GC, MyCleanup) A[Np];
    ll* chemistory = new(GC) ll[Nc];
    // ll* physics = new(GC) ll[Np];
    // ll* chemistory = new(GC) ll[Nc];
    // ll* ave = new ll[N];

    // 100点満点で点数を入力
    for (ll k = 0; k < Np; k++)
    {
      physics[k] = rand() % 101;
    }
    for (ll k = 0; k < Nc; k++)
    {
      chemistory[k] = rand() % 101;
    }

    // 平均を出す。
    for (ll p = 0; p < Np; p++)
    {
      for (ll q = 0; q < Nc; q + 8) //cache lineの大きさが64B、ll intの大きさが8Bなので８ずつアクセスすれば配列の持つ全てのメモリ空間にアクセスできる。
      {
        ll res = (physics[p] + chemistory[q]) / 2;
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
    delete[] physics;
    delete[] chemistory;
    // delete[] ave;
  }

  printf("GC count = %lu\n", GC_get_gc_no());
  printf("Heap size that GC managed = %lu\n", GC_get_heap_size());
}