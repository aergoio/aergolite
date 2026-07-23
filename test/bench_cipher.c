/*
** bench_cipher - measure raw throughput of the transport/disk ciphers.
**
** This benchmark compares the two ciphers available in the codec
** (core/xrc4.c and core/chacha.c) on the two workloads that matter for
** AergoLite:
**
**   - disk pages: a few KiB per page (SQLite default page size is 4096),
**     encrypted many times under the same key (the sbox/key is set up once
**     and reused for every page).
**   - transport messages: small frames, also under a long-lived key.
**
** It measures the steady-state cipher throughput (MB/s) for several sizes,
** plus the one-time key setup cost (xrc4_init) which is amortized across
** all pages/messages that share a key.
**
** Build:  make bench
** Run:    ./test/bench_cipher
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>

/* include the cipher implementations directly so the benchmark does not
** depend on the shared library (and exercises the exact same code path). */
#include "../core/xrc4.c"
#include "../core/chacha.c"

#define MB (1024.0 * 1024.0)

/* sizes to benchmark: small transport frames up to a large steady-state buffer */
static const unsigned int sizes[] = {
  64, 256, 1024, 4096, 16384, 65536, 262144, 1048576
};
#define N_SIZES (int)(sizeof(sizes)/sizeof(sizes[0]))

/* target bytes processed per measurement so timing is stable across sizes */
#define TARGET_BYTES (256 * 1024 * 1024)

static double now_seconds(void){
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static unsigned int pick_iters(unsigned int size){
  unsigned long iters = TARGET_BYTES / size;
  if( iters < 100 ) iters = 100;
  if( iters > 5000000 ) iters = 5000000;
  return (unsigned int)iters;
}

/* measure xrc4 throughput: sbox is set up once (as for disk pages sharing a
** key), then xrc4_crypt is the per-message hot path. */
static double bench_xrc4(unsigned int size, unsigned int iters,
                         unsigned char *out, const unsigned char *in,
                         const unsigned char *sbox, unsigned char *iv){
  unsigned int i;
  double t0, t1;
  /* warmup */
  xrc4_crypt(out, (unsigned char*)in, size, (unsigned char*)sbox, iv, 8, 1);
  t0 = now_seconds();
  for( i=0; i<iters; i++ ){
    xrc4_crypt(out, (unsigned char*)in, size, (unsigned char*)sbox, iv, 8, 1);
  }
  t1 = now_seconds();
  if( t1<=t0 ) return -1.0;
  return ((double)size * iters) / MB / (t1 - t0);
}

/* measure chacha throughput: each call sets up the 512-bit state from
** key+iv+counter (no precomputation), which is the real per-message cost. */
static double bench_chacha(unsigned int size, unsigned int iters,
                           unsigned char *out, const unsigned char *in,
                           const unsigned char *key, unsigned char *iv){
  unsigned int i;
  double t0, t1;
  /* warmup */
  chacha_encrypt(out, in, size, (unsigned char*)key, iv, 8, 1, 20);
  t0 = now_seconds();
  for( i=0; i<iters; i++ ){
    chacha_encrypt(out, in, size, (unsigned char*)key, iv, 8, 1, 20);
  }
  t1 = now_seconds();
  if( t1<=t0 ) return -1.0;
  return ((double)size * iters) / MB / (t1 - t0);
}

int main(void){
  unsigned char *in, *out, sbox[256], key[32], iv[8];
  unsigned int s, iters;
  double t_xrc4, t_chacha, init_us;
  double t0, t1;
  int i;

  /* prepare key material */
  for( i=0; i<32; i++ ) key[i] = (unsigned char)(i * 7 + 1);
  for( i=0; i<8; i++ ) iv[i] = (unsigned char)(i * 3 + 1);

  /* one-time xrc4 key setup (amortized across all pages/messages) */
  t0 = now_seconds();
  xrc4_init(key, 32, sbox);
  t1 = now_seconds();
  init_us = (t1 - t0) * 1e6;

  /* largest size drives the allocation */
  in  = (unsigned char*)malloc(sizes[N_SIZES-1]);
  out = (unsigned char*)malloc(sizes[N_SIZES-1]);
  if( !in || !out ){
    fprintf(stderr, "out of memory\n");
    return 1;
  }
  for( i=0; i<(int)sizes[N_SIZES-1]; i++ ) in[i] = (unsigned char)(i & 0xff);

  printf("cipher throughput benchmark (ChaCha20 rounds=20, IV=8 bytes)\n");
  printf("xrc4_init one-time key setup: %.2f us (amortized per key)\n\n", init_us);
  printf(" %10s | %10s | %14s | %14s | %8s\n",
         "size", "iters", "xrc4 MB/s", "chacha20 MB/s", "ratio");
  printf("-----------+------------+----------------+----------------+--------\n");

  for( s=0; s<N_SIZES; s++ ){
    iters = pick_iters(sizes[s]);
    t_xrc4   = bench_xrc4(sizes[s], iters, out, in, sbox, iv);
    t_chacha = bench_chacha(sizes[s], iters, out, in, key, iv);
    printf(" %10u | %10u | %14.1f | %14.1f | %7.2fx\n",
           sizes[s], iters, t_xrc4, t_chacha,
           (t_chacha>0.0 && t_xrc4>0.0) ? t_chacha / t_xrc4 : 0.0);
  }

  free(in);
  free(out);
  return 0;
}
