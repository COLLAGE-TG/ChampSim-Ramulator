#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

using namespace std;
using ll = long long;

const ll Np = 100;
const ll Nc = 1000000;
const ll M = 10; //繰り返し

int main(void)
{

  for (ll i = 0;i < M;i++)
  {
    ll* physics = new ll[Np];
    ll* chemistory = new ll[Nc];
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
}