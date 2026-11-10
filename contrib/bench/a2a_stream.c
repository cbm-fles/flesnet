// SPDX-License-Identifier: GPL-3.0-only
//
// Streaming all-to-all bandwidth: every rank continuously sends a block of
// m bytes to every other rank and receives one from each, with up to w rounds
// in flight (non-blocking, no barrier between rounds). Unlike osu_alltoall,
// there is no self-message copy in the critical path, which matches the
// network part of timeslice building. With -s, a memcpy of the own block is
// added per round (concurrent with the transfers) for comparison.
//
// Build: mpicc -O2 -o a2a_stream a2a_stream.c
// Usage: a2a_stream [-m min[:max]] [-w window] [-t seconds] [-s]
// Sizes double from min to max.
// Output per size: network throughput per node (min/mean/max over ranks),
// i.e. (N-1)*m per round and node, in GB/s (1e9).

#include <mpi.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static size_t parse_size(const char* s) {
  char* end;
  double v = strtod(s, &end);
  switch (*end) {
  case 'k':
  case 'K':
    v *= 1024.0;
    break;
  case 'm':
  case 'M':
    v *= 1024.0 * 1024.0;
    break;
  case 'g':
  case 'G':
    v *= 1024.0 * 1024.0 * 1024.0;
    break;
  default:
    break;
  }
  return (size_t)v;
}

static void* alloc(size_t n) {
  void* p = NULL;
  if (posix_memalign(&p, 4096, n) != 0) {
    fprintf(stderr, "allocation of %zu bytes failed\n", n);
    MPI_Abort(MPI_COMM_WORLD, 1);
  }
  memset(p, 1, n); // fault in the pages before measuring
  return p;
}

// Run exactly `rounds` rounds and return the elapsed time.
static double run(size_t m,
                  int window,
                  long rounds,
                  int self_copy,
                  int rank,
                  int size,
                  char* sendbuf,
                  char* recvbuf,
                  char* selfbuf) {
  int peers = size - 1;
  MPI_Request* req = malloc(sizeof(MPI_Request) * 2 * peers * window);
  for (int i = 0; i < 2 * peers * window; ++i) {
    req[i] = MPI_REQUEST_NULL;
  }

  MPI_Barrier(MPI_COMM_WORLD);
  double t0 = MPI_Wtime();
  for (long n = 0; n < rounds; ++n) {
    int slot = n % window;
    MPI_Request* r = req + 2 * peers * slot;
    MPI_Waitall(2 * peers, r, MPI_STATUSES_IGNORE);
    for (int i = 1; i <= peers; ++i) {
      int src = (rank - i + size) % size;
      int dst = (rank + i) % size;
      char* rb = recvbuf + ((size_t)slot * peers + (i - 1)) * m;
      char* sb = sendbuf + (size_t)(i - 1) * m;
      MPI_Irecv(rb, (int)m, MPI_BYTE, src, 0, MPI_COMM_WORLD, &r[2 * (i - 1)]);
      MPI_Isend(sb, (int)m, MPI_BYTE, dst, 0, MPI_COMM_WORLD,
                &r[2 * (i - 1) + 1]);
    }
    if (self_copy) {
      memcpy(selfbuf, sendbuf, m);
    }
  }
  MPI_Waitall(2 * peers * window, req, MPI_STATUSES_IGNORE);
  double elapsed = MPI_Wtime() - t0;
  free(req);
  return elapsed;
}

int main(int argc, char** argv) {
  MPI_Init(&argc, &argv);
  int rank, size;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  size_t min_size = 1 << 20, max_size = 512u << 20;
  int window = 2;
  double seconds = 10.0;
  int self_copy = 0;
  int opt;
  while ((opt = getopt(argc, argv, "m:w:t:s")) != -1) {
    switch (opt) {
    case 'm': {
      char* colon = strchr(optarg, ':');
      min_size = parse_size(optarg);
      max_size = colon ? parse_size(colon + 1) : min_size;
      break;
    }
    case 'w':
      window = atoi(optarg);
      break;
    case 't':
      seconds = atof(optarg);
      break;
    case 's':
      self_copy = 1;
      break;
    default:
      if (rank == 0) {
        fprintf(stderr,
                "usage: %s [-m min[:max]] [-w window] [-t seconds] [-s]\n",
                argv[0]);
      }
      MPI_Abort(MPI_COMM_WORLD, 2);
    }
  }
  if (size < 2 || window < 1 || max_size > (size_t)INT32_MAX) {
    if (rank == 0) {
      fprintf(stderr, "need >= 2 ranks, window >= 1, size < 2 GiB\n");
    }
    MPI_Abort(MPI_COMM_WORLD, 2);
  }

  int peers = size - 1;
  char* sendbuf = alloc(max_size * peers);
  char* recvbuf = alloc(max_size * peers * window);
  char* selfbuf = self_copy ? alloc(max_size) : NULL;

  if (rank == 0) {
    printf("# a2a_stream: %d ranks, window %d, %.0f s per size%s\n", size,
           window, seconds, self_copy ? ", with self copy" : "");
    printf("# %12s %10s %10s %10s %10s\n", "size", "rounds", "min_GBps",
           "mean_GBps", "max_GBps");
  }
  for (size_t m = min_size; m <= max_size; m *= 2) {
    // calibrate the round time (doubles as warmup), then run a fixed number
    // of rounds so that all ranks post matching operations
    long cal = 2 * window + 2;
    double t =
        run(m, window, cal, self_copy, rank, size, sendbuf, recvbuf, selfbuf);
    double tmax;
    MPI_Allreduce(&t, &tmax, 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);
    long rounds = (long)(seconds / (tmax / cal));
    if (rounds < cal) {
      rounds = cal;
    }
    MPI_Bcast(&rounds, 1, MPI_LONG, 0, MPI_COMM_WORLD);
    double elapsed = run(m, window, rounds, self_copy, rank, size, sendbuf,
                         recvbuf, selfbuf);
    double bw = (double)rounds * peers * m / elapsed / 1e9;
    double bmin, bmax, bsum;
    MPI_Reduce(&bw, &bmin, 1, MPI_DOUBLE, MPI_MIN, 0, MPI_COMM_WORLD);
    MPI_Reduce(&bw, &bmax, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&bw, &bsum, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    if (rank == 0) {
      printf("%14zu %10ld %10.2f %10.2f %10.2f\n", m, rounds, bmin, bsum / size,
             bmax);
      fflush(stdout);
    }
    if (m > max_size / 2) {
      break;
    }
  }
  free(sendbuf);
  free(recvbuf);
  free(selfbuf);
  MPI_Finalize();
  return 0;
}
