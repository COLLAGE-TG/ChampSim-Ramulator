#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <gc/gc.h>

double cur_time()
{
  struct timeval tp[1];
  gettimeofday(tp, 0);
  return tp->tv_sec + tp->tv_usec * 1.0e-6;
}

void* alloc(long sz)
{
  void* p = GC_MALLOC(sz);
  assert(p);
  return p;
}

int main(int argc, char** argv)
{
  GC_INIT();

  long sz = (argc > 1 ? atol(argv[1]) : 1000); /* cell size */
  long l = (argc > 2 ? atol(argv[2]) : 12345); /* length of the array */
  long m = (argc > 3 ? atol(argv[3]) : 123456); /* the number of times a list is made */
  long d = (argc > 4 ? atol(argv[4]) : 1);

  GC_set_free_space_divisor(d);

  printf("object size = %ld bytes\n", sz);
  printf("array length = %ld\n", l);
  printf("iterations = %ld\n", m);
  printf("minimum = %ld * %ld + %ld * %ld = %ld\n",
         sizeof(void*), l, sz, l, sizeof(void*) * l + sz * l);
  printf("total alloc = %ld * %ld + %ld x %ld = %ld\n",
         sizeof(void*), l, sz, m, sizeof(void*) * l + sz * m);

  double t0 = cur_time();
  long i;
  /* create an array holding pointers to l objects */
  void** a = (void**)alloc(sizeof(void*) * l);
  /* create an object m times, keeping l of them alive */
  for (i = 0; i < m; i++)
  {
    a[i % l] = alloc(sz);
  }
  double t1 = cur_time();
  double dt = t1 - t0;
  printf("%f sec for %ld allocs (%f nsec per alloc)\n",
         dt, m, dt * 1.0e9 / m);
  printf("GC count = %lu\n", GC_get_gc_no());
  printf("Heap size that GC managed = %lu\n", GC_get_heap_size());
  return 0;
}