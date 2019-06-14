// Copyright (c) Gaëtan Leurent and Thomas Peyrin, 2018-2019

// To the extent possible under law, the author(s) have dedicated all
// copyright and related and neighboring rights to this software to the
// public domain worldwide. This software is distributed without any
// warranty.  You should have received a copy of the CC0 Public Domain
// Dedication along with this software. If not, see
// <http://creativecommons.org/publicdomain/zero/1.0/>.


#define _XOPEN_SOURCE 600

#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <assert.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <sys/mman.h>

#include <time.h>

#define tablength(t) (sizeof((t))/sizeof((t)[0]))

uint32_t rotate_left(uint32_t x, int n) {
  n&=31;
  if (n==0)
    return n;
  return (x<<n) | (x>>(32-n));
}

typedef uint64_t word;

typedef struct {
  uint32_t x[5];
  double p;
} __attribute__((packed)) data_t;

int cmp_array(const void *A, const void *B) {
  const uint32_t *a = A;
  const uint32_t *b = B;
  for (int i=0; i<5; i++) {
    if (a[i] > b[i])
      return 1;
    else if (a[i] < b[i])
      return -1;
  }
  return 0;
}

// Binary search
data_t *find (uint32_t x[5], data_t *diffset, size_t min, size_t max) {
  if (min >= max)
    return NULL;
  int mid = (min+max)/2;

  int c = cmp_array(&diffset[mid].x, x);
  if (c == 0)
    return &diffset[mid];
  if (c > 0)
    return find(x, diffset, min, mid);
  if (c < 0)
    return find(x, diffset, mid+1, max);
}

struct edge { double c, p; };
struct edge2 { double c, p; uint32_t data[5]; };

void printf_diff(uint32_t a, uint32_t b, int rot, FILE *out) {
  a = (a<<rot) | (a>>(32-rot));
  b = (b<<rot) | (b>>(32-rot));

  for (int i=31; i>=0; i--) {
    int idx = 2*((a>>i)&1) + ((b>>i)&1);
    putc(((char[]){'0', 'n', 'u', '1'}[idx]), out);
  }
}

void print_template (char template[], uint32_t IV1[5], uint32_t IV2[5], int z, FILE *out) {
  z = ~z;
  
  FILE *t = fopen(template, "r");
  // FSM to find pattern '00:'
  int state = 0;
  while (state != 3) {
    switch (fgetc(t)) {
    case '0':
      if (state < 2)
	state ++;
      else
	state = 0;
      break;
    case ':':
      if (state == 2)
	state ++;
      else
	state = 0;
      break;
    default:
      state = 0;
    }
  }
  fseek(t, 33, SEEK_CUR);
  
  // Header  
  fprintf (out, "80\n");
  fprintf (out, "                  A[i]                                W[i]               Dw     Pu[i]       Pc[i]      N[i]  \n");

  // Initial state
  fprintf (out, "\n-4: "); printf_diff(IV1[4], IV2[4], 2, out);
  fprintf (out, "\n-3: "); printf_diff(IV1[3], IV2[3], 2, out);
  fprintf (out, "\n-2: "); printf_diff(IV1[2], IV2[2], 2, out);
  fprintf (out, "\n-1: "); printf_diff(IV1[1], IV2[1], 0, out);
  fprintf (out, "\n00: "); printf_diff(IV1[0], IV2[0], 0, out);

  // Template
  int c;
  while ((c = fgetc(t)) != EOF) { putc(c,out); };

  // Extra conditions
  fprintf (out, "79[4] = %i\n", ((z<<=1)>>13)&1); // z13
  fprintf (out, "79[2] = %i\n", ((z<<=1)>>13)&1); // z12
  fprintf (out, "78[7] = %i\n", ((z<<=1)>>13)&1); // z11
  fprintf (out, "78[3] = %i\n", ((z<<=1)>>13)&1); // z10
  fprintf (out, "78[0] = %i\n", ((z<<=1)>>13)&1); // z9

  fprintf (out, "73[2] = %i\n", ((z<<=1)>>13)&1); // z1
  fprintf (out, "76[3] = %i\n", ((z<<=1)>>13)&1); // z7
  fprintf (out, "76[1] = %i\n", ((z<<=1)>>13)&1); // z2
  fprintf (out, "76[0] = %i\n", ((z<<=1)>>13)&1); // z3
  fprintf (out, "77[8] = %i\n", ((z<<=1)>>13)&1); // z8
  fprintf (out, "77[2] = %i\n", ((z<<=1)>>13)&1); // z6
  fprintf (out, "77[1] = %i\n", ((z<<=1)>>13)&1); // z4
  fprintf (out, "77[0] = %i\n", ((z<<=1)>>13)&1); // z5
}


// Main differences (fixed signs in last two rounds)
struct diff_group {
  int z;
  struct {double p; uint32_t diff[5];} g[38];
};

struct diff_group diff_groups[] = {
#include "diff_groups.h"
};

int main(int argc, char *argv[]) {

  char* infile = NULL;
  char* template = NULL;
  uint32_t indiff[5], IV1[5], IV2[5];
  int random = 1;
  int do_simulation = 0;
  FILE* out = stdout;

  srand(getpid()*0xdeadbeef + time(NULL)*0xbadc0ffe);
  
  for (int i=1; i<argc; i++) {
    if (argv[i][0] != '-') {
    USAGE:
      printf ("Usage: %s -s<diffset> [-o<out>] [-t<template>] [-d<diff>] [-b]\n"
	      "  -s: difference set\n"
	      "  -o: output file (default stdout)\n"
	      "  -t: path template\n"
	      "  -d: input difference (default: random)\n"
	      "      Ex: ffffda04/fffffed4/fffffffc/fffffff8/00000000\n"
	      "  -b: simulate blocks\n"

	      , argv[0]);
      return -1;
    }
    switch (argv[i][1]) {
    case 's':
      infile = argv[i]+2;
      break;
    case 't':
      template = argv[i]+2;
      break;
    case 'o':
      out = fopen(argv[i]+2, "w");
      if (!out) {
	perror("Can't open output file");
	exit(-1);
      }
      break;
    case 'd':
      if (strlen(argv[i]) != 2+5*8+4)
	goto USAGE;
      if (argv[i][10] != '/' || argv[i][19] != '/' || argv[i][28] != '/' || argv[i][37] != '/')
	goto USAGE;
      indiff[0] = strtoul(argv[i]+2 , NULL, 16);
      indiff[1] = strtoul(argv[i]+11, NULL, 16);
      indiff[2] = strtoul(argv[i]+20, NULL, 16);
      indiff[3] = strtoul(argv[i]+29, NULL, 16);
      indiff[4] = strtoul(argv[i]+38, NULL, 16);
      random = 0;
      break;
    case 'b':
      do_simulation = 1;
      break;
    default:
      goto USAGE;
    }
  }
  if (!infile)
    goto USAGE;

  /**********************************************
   * Prepare internal structures
   **********************************************/

  
  // Build all differences reachable with 1 block

  // Adjust proba
  double MARC_COST = INFINITY;
  for (unsigned i=0; i<tablength(diff_groups); i++) {
    for (unsigned j=0; j<tablength(diff_groups[0].g); j++) {
      if (diff_groups[i].g[j].p && diff_groups[i].g[j].p < MARC_COST)
	MARC_COST = diff_groups[i].g[j].p;
    }
  }
  
  for (unsigned i=0; i<tablength(diff_groups); i++) {
    for (unsigned j=0; j<tablength(diff_groups[0].g); j++) {
      if (diff_groups[i].g[j].p) {
	double p = exp2(diff_groups[i].g[j].p - MARC_COST);
	diff_groups[i].g[j].p = 1/p;
      }
    }
  }

  // Alt differeneces: changing signs in last rounds
  struct data_z {
    uint32_t data[5];
    int z;
  };

  // Base diffs
  struct data_z alt_diff[] = {
    // Message flips
    {{(1<<6), (1<<1), 0, 0, 0},  (1<< 8)}, // z9
    {{(1<<9), (1<<4), 0, 0, 0},  (1<< 9)}, // z10
    {{(1<<13), (1<<8), 0, 0, 0}, (1<<10)}, // z11
    {{(1<<3), 0, 0, 0, 0},       (1<<11)}, // z12
    {{(1<<5), 0, 0, 0, 0},       (1<<12)}, // z13
  };

  // Build combinaisons of base diffs
  struct data_z alt_diffs[1<<tablength(alt_diff)];

  for (unsigned j=0; j < 1<<tablength(alt_diff); j++) {
    struct data_z d = {{0}, 0};
    for (unsigned u=0; u < tablength(alt_diff); u++) {
      if (j & (1<<u)) {
	d.z |= alt_diff[u].z;
	for (int x=0; x<5; x++) {
	  d.data[x] += alt_diff[u].data[x];
	}
      }
    }
    alt_diffs[j] = d;
  }
  
  printf ("Reading %s\n", infile);
  FILE* f = fopen(infile, "r");
  if (!f) {
  READ_ERROR:
    perror("Error reading file");
    exit (-2);
  }
  int fd = fileno(f);
  struct stat st;
  int r = fstat(fd, &st);
  if (r)
    goto READ_ERROR;
  
  data_t *diffset;
  size_t diffset_n = st.st_size/sizeof(diffset[0]);
  diffset = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (diffset == MAP_FAILED) {
    perror("mmap failed");
    exit (-1);
  }
    
  /**********************************************
   * Set up IV
   **********************************************/

  // Difference
  if (random) {
    printf ("Starting from a random input difference in G\n");
    int r = rand() % diffset_n;
    memcpy(indiff, diffset[r].x, sizeof(indiff));
  }

  // Values
  for (int i=0; i<5; i++) {
    IV1[i] = (rand()<<16) + rand();
    IV2[i] = IV1[i] + indiff[i];
  }

  int nblocks = 0;
  uint64_t total_cost = 0;
  
  void simulate_block(uint32_t IV1[5], uint32_t IV2[5]) {  

    /**********************************************
     * Look up difference
     **********************************************/

    indiff[0] = IV2[0] - IV1[0];
    indiff[1] = IV2[1] - IV1[1];
    indiff[2] = IV2[2] - IV1[2];
    indiff[3] = IV2[3] - IV1[3];
    indiff[4] = IV2[4] - IV1[4];

    if (!indiff[0] && !indiff[1] && !indiff[2] && !indiff[3] && !indiff[4]) {
      printf ("Collision found!\n");
      printf ("Total cost: %f Cblock\n", total_cost/exp2(MARC_COST));
      printf ("Number of near-collision blocks: %i\n", nblocks);
      exit (0);
    }

    printf ("Block #%i\n", ++nblocks);
    printf ("  Input diff [%08x %08x %08x %08x %08x]\n",
	    indiff[0], indiff[1], indiff[2], indiff[3], indiff[4]);
    data_t *d0 = find(indiff, diffset, 0, diffset_n);
    if (d0) {
      printf ("  Cost %f (precomputed)\n", d0->p);
    } else {
      printf ("  Not found!\n");
      exit (-1);
    }


    double bestp = INFINITY;
    int bestd = -1;
    int besti = -1;

    // Reevaluate cost with clustering
    for (word d=0; d<tablength(alt_diffs); d++) {
      uint32_t sum2[5];
      for (int t=0; t<5; t++)
	sum2[t] = indiff[t] + alt_diffs[d].data[t];

      unsigned istart = 0, istop = tablength(diff_groups);
      for (unsigned i=istart; i<istop; i++) {
	int n_p = 0;
	struct edge group_p[tablength(diff_groups[0].g)];
	for (unsigned j=0; j<tablength(diff_groups[0].g); j++) {
	  if (diff_groups[i].g[j].p) {
	    uint32_t sum3[5];
	    for (int t=0; t<5; t++)
	      sum3[t] = sum2[t] + diff_groups[i].g[j].diff[t];
	    data_t *c = find(sum3, diffset, 0, diffset_n);
	    group_p[n_p++] = (struct edge){ c: c? c->p: INFINITY, p: diff_groups[i].g[j].p };
	  }
	}
	// qsort(group_p, n_p, sizeof(group_p[0]), cmp_pair_double);
	// External call to qsort is too slow, do a quadratic sort instead
	struct edge group_p_sorted[tablength(diff_groups[0].g)];
	for (int n=0; n<n_p; n++) {
	  int min_idx = 0;
	  for (int m=0; m<n_p; m++) {
	    if (group_p[m].c < group_p[min_idx].c)
	      min_idx = m;
	  }
	  group_p_sorted[n] = group_p[min_idx];
	  if (group_p[min_idx].c == INFINITY)
	    break;
	  else
	    group_p[min_idx].c = INFINITY;
	}
	    
	double sum_p = 0;
	double wsum_p = 0;
	/* printf ("Group %i/%i:", (int)d, i); */
	for (int n=0; n<n_p; n++) {
	  if (n && group_p_sorted[n].c >= (wsum_p+1)/sum_p-0.0000000001)
	    break; // Remaining choices make it worse
	  sum_p  += group_p_sorted[n].p;
	  wsum_p += group_p_sorted[n].p*group_p_sorted[n].c;
	}
	double newp = (wsum_p+1)/sum_p;

	if (newp <= bestp) {
	  bestp = newp;
	  bestd = d;
	  besti = i;
	}
      }
    }

    int z = alt_diffs[bestd].z | diff_groups[besti].z;
    {
      int zz = ~z;
      printf ("  Z conditions: z13=%i", ((zz<<=1)>>13)&1);
      printf (" z12=%i", ((zz<<=1)>>13)&1);
      printf (" z11=%i", ((zz<<=1)>>13)&1);
      printf (" z10=%i", ((zz<<=1)>>13)&1);
      printf (" z9=%i", ((zz<<=1)>>13)&1);
      printf (" z1=%i", ((zz<<=1)>>13)&1);
      printf (" z7=%i", ((zz<<=1)>>13)&1);
      printf (" z2=%i", ((zz<<=1)>>13)&1);
      printf (" z3=%i", ((zz<<=1)>>13)&1);
      printf (" z8=%i", ((zz<<=1)>>13)&1);
      printf (" z6=%i", ((zz<<=1)>>13)&1);
      printf (" z4=%i", ((zz<<=1)>>13)&1);
      printf (" z5=%i", ((zz<<=1)>>13)&1);
      printf ("\n");
    }
    
    // Print best group
    printf ("  Useful output diffs:\n");
    word d = bestd;
    unsigned i = besti;

    int n_p = 0;
    struct edge2 group_p[tablength(diff_groups[0].g)];
    for (unsigned j=0; j<tablength(diff_groups[0].g); j++) {
      if (diff_groups[i].g[j].p) {
	uint32_t sum3[5];
	for (int t=0; t<5; t++)
	  sum3[t] = indiff[t] + alt_diffs[d].data[t] + diff_groups[i].g[j].diff[t];
	data_t *c = find(sum3, diffset, 0, diffset_n);
	group_p[n_p++] = (struct edge2){ c: c? c->p: INFINITY, p: diff_groups[i].g[j].p };
	for (int t=0; t<5; t++)
	  group_p[n_p-1].data[t] = alt_diffs[d].data[t] + diff_groups[i].g[j].diff[t];
      }
    }
    // qsort(group_p, n_p, sizeof(group_p[0]), cmp_pair_double);
    // External call to qsort is too slow, do a quadratic sort instead
    struct edge2 group_p_sorted[tablength(diff_groups[0].g)];
    for (int n=0; n<n_p; n++) {
      int min_idx = 0;
      for (int m=0; m<n_p; m++) {
	if (group_p[m].c < group_p[min_idx].c)
	  min_idx = m;
      }
      group_p_sorted[n] = group_p[min_idx];
      if (group_p[min_idx].c == INFINITY)
	break;
      else
	group_p[min_idx].c = INFINITY;
    }
	    
    double sum_p = 0;
    double wsum_p = 0;
    int usefull_diffs = 0;
    for (int n=0; n<n_p; n++,usefull_diffs++) {
      if (n && group_p_sorted[n].c >= (wsum_p+1)/sum_p-0.0000000001)
	break; // Remaining choices make it worse
      printf ("    %08x %08x %08x %08x %08x", group_p_sorted[n].data[0], group_p_sorted[n].data[1], group_p_sorted[n].data[2], group_p_sorted[n].data[3], group_p_sorted[n].data[4]);
      printf (": Edge cost=%f, Remaining cost=%f", 1/group_p_sorted[n].p, group_p_sorted[n].c);
      printf (" [=> %08x %08x %08x %08x %08x]\n", group_p_sorted[n].data[0]+indiff[0], group_p_sorted[n].data[1]+indiff[1], group_p_sorted[n].data[2]+indiff[2], group_p_sorted[n].data[3]+indiff[3], group_p_sorted[n].data[4]+indiff[4]);
      sum_p  += group_p_sorted[n].p;
      wsum_p += group_p_sorted[n].p*group_p_sorted[n].c;
    }
    assert(bestp == (wsum_p+1)/sum_p);
  
    printf ("  Cost %f (computed from usefull diffs)\n", bestp);
    if (template)
      print_template(template, IV1, IV2, z, out);
    /* printf ("  Alt diff %i: %08x %08x %08x %08x %08x\n", bestd,  */
    /* 	  alt_diffs[bestd].data[0], alt_diffs[bestd].data[1], alt_diffs[bestd].data[2], alt_diffs[bestd].data[3], alt_diffs[bestd].data[4]); */
    /* printf ("  Diff group %i\n", besti); */
    /* printf ("  Z: %#x [%#x/%#x]\n", z, alt_diffs[bestd].z, diff_groups[besti].z); */

    if (do_simulation) {

      printf ("  Simulating near-collision block:\n");
      for (int t=0;; t++) {
	uint32_t a1 = (rand()<<16) + rand();
	uint32_t b1 = (rand()<<16) + rand();
	uint32_t c1 = (rand()<<16) + rand();
	uint32_t d1 = (rand()<<16) + rand();
	uint32_t e1 = (rand()<<16) + rand();

	uint32_t a2 = a1;
	uint32_t b2 = b1;
	uint32_t c2 = c1;
	uint32_t d2 = d1;
	uint32_t e2 = e1;

	uint32_t m[16]; // Last 16 message words
	for (int i=0; i<16; i++)
	  m[i] = (rand()<<16) + rand();

#define setm(i,j,z) m[(i)-64] &= ~(1U<<(j)), m[(i)-64] |= (z)<<(j)

	// Apply zi conditions
	int zz = ~z;
	setm(79, 4, ((zz<<=1)>>13)&1); // z13
	setm(79, 2, ((zz<<=1)>>13)&1); // z12
	setm(78, 7, ((zz<<=1)>>13)&1); // z11
	setm(78, 3, ((zz<<=1)>>13)&1); // z10
	setm(78, 0, ((zz<<=1)>>13)&1); // z9
	setm(73, 2, ((zz<<=1)>>13)&1); // z1
	setm(76, 3, ((zz<<=1)>>13)&1); // z7
	setm(76, 1, ((zz<<=1)>>13)&1); // z2
	setm(76, 0, ((zz<<=1)>>13)&1); // z3
	setm(77, 8, ((zz<<=1)>>13)&1); // z8
	setm(77, 2, ((zz<<=1)>>13)&1); // z6
	setm(77, 1, ((zz<<=1)>>13)&1); // z4
	setm(77, 0, ((zz<<=1)>>13)&1); // z5

	// Apply message conditions
	setm(68,  5, (~m[3]>>0)&1);  // 67[0]+68[5] = 1
	setm(72, 30, (~m[3]>>0)&1);  // 67[0]+72[30] = 1
	setm(71,  6, (~m[6]>>1)&1);  // 70[1]+71[6] = 1
	setm(72,  5, (~m[7]>>0)&1);  // 71[0]+72[5] = 1
	setm(76, 30, (~m[7]>>0)&1);  // 71[0]+76[30] = 1
	setm(74,  7, (~m[9]>>2)&1);  // 73[2]+74[7] = 1
	setm(75,  6, (~m[10]>>1)&1); // 74[1]+75[6] = 1
	setm(76,  6, (~m[11]>>1)&1); // 75[1]+76[6] = 1

    
#define sha1_round4(a,b,c,d,e,m)					\
	rotate_left(a,5) + (b^rotate_left(c,30)^rotate_left(d,30)) + rotate_left(e,30) + m + 0xCA62C1D6;
    
	e1 = sha1_round4(a1, b1, c1, d1, e1, m[0]);
	d1 = sha1_round4(e1, a1, b1, c1, d1, m[1]);
	c1 = sha1_round4(d1, e1, a1, b1, c1, m[2]);
	b1 = sha1_round4(c1, d1, e1, a1, b1, m[3]);
	a1 = sha1_round4(b1, c1, d1, e1, a1, m[4]);

	e1 = sha1_round4(a1, b1, c1, d1, e1, m[5]);
	d1 = sha1_round4(e1, a1, b1, c1, d1, m[6]);
	c1 = sha1_round4(d1, e1, a1, b1, c1, m[7]);
	b1 = sha1_round4(c1, d1, e1, a1, b1, m[8]);
	a1 = sha1_round4(b1, c1, d1, e1, a1, m[9]);

	e1 = sha1_round4(a1, b1, c1, d1, e1, m[10]);
	d1 = sha1_round4(e1, a1, b1, c1, d1, m[11]);
	c1 = sha1_round4(d1, e1, a1, b1, c1, m[12]);
	b1 = sha1_round4(c1, d1, e1, a1, b1, m[13]);
	a1 = sha1_round4(b1, c1, d1, e1, a1, m[14]);

	e1 = sha1_round4(a1, b1, c1, d1, e1, m[15]);

	m[ 0] ^= 0b00000000000000000000000000000000;
	m[ 1] ^= 0b00000000000000000000000000000000;
	m[ 2] ^= 0b00000000000000000000000000000000;
	m[ 3] ^= 0b00000000000000000000000000000001;
	m[ 4] ^= 0b00000000000000000000000000100000;
	m[ 5] ^= 0b00000000000000000000000000000001;
	m[ 6] ^= 0b01000000000000000000000000000010;
	m[ 7] ^= 0b01000000000000000000000001000001;
	m[ 8] ^= 0b01000000000000000000000000100010;
	m[ 9] ^= 0b10000000000000000000000000000101;
	m[10] ^= 0b11000000000000000000000010000010;
	m[11] ^= 0b11000000000000000000000001000110;
	m[12] ^= 0b01000000000000000000000001001011;
	m[13] ^= 0b10000000000000000000000100000111;
	m[14] ^= 0b00000000000000000000000010001001;
	m[15] ^= 0b00000000000000000000000000010100;

    
	e2 = sha1_round4(a2, b2, c2, d2, e2, m[0]);
	d2 = sha1_round4(e2, a2, b2, c2, d2, m[1]);
	c2 = sha1_round4(d2, e2, a2, b2, c2, m[2]);
	b2 = sha1_round4(c2, d2, e2, a2, b2, m[3]);
	a2 = sha1_round4(b2, c2, d2, e2, a2, m[4]);

	e2 = sha1_round4(a2, b2, c2, d2, e2, m[5]);
	d2 = sha1_round4(e2, a2, b2, c2, d2, m[6]);
	c2 = sha1_round4(d2, e2, a2, b2, c2, m[7]);
	b2 = sha1_round4(c2, d2, e2, a2, b2, m[8]);
	a2 = sha1_round4(b2, c2, d2, e2, a2, m[9]);

	e2 = sha1_round4(a2, b2, c2, d2, e2, m[10]);
	d2 = sha1_round4(e2, a2, b2, c2, d2, m[11]);
	c2 = sha1_round4(d2, e2, a2, b2, c2, m[12]);
	b2 = sha1_round4(c2, d2, e2, a2, b2, m[13]);
	a2 = sha1_round4(b2, c2, d2, e2, a2, m[14]);

	e2 = sha1_round4(a2, b2, c2, d2, e2, m[15]);
    
	uint32_t dd = rotate_left(d2,30) - rotate_left(d1,30);
	uint32_t dc = rotate_left(c2,30) - rotate_left(c1,30);
	uint32_t db = rotate_left(b2,30) - rotate_left(b1,30);
	uint32_t da = a2 - a1;
	uint32_t de = e2 - e1;

	for (int n=0; n<usefull_diffs; n++) {
	  if (group_p_sorted[n].data[0] == de &&
	      group_p_sorted[n].data[1] == da &&
	      group_p_sorted[n].data[2] == db &&
	      group_p_sorted[n].data[3] == dc &&
	      group_p_sorted[n].data[4] == dd) {
	    printf ("Block found!\n  Output diff [%08x %08x %08x %08x %08x]\n", de, da, db, dc, dd);
	    printf ("  Simulated cost: %f Cblock\n\n", t/exp2(MARC_COST));
	    total_cost += t;
	    uint32_t newIV1[5], newIV2[5];
	    newIV1[0] = IV1[0] + e1;
	    newIV1[1] = IV1[1] + a1;
	    newIV1[2] = IV1[2] + rotate_left(b1,30);
	    newIV1[3] = IV1[3] + rotate_left(c1,30);
	    newIV1[4] = IV1[4] + rotate_left(d1,30);

	    newIV2[0] = IV2[0] + e2;
	    newIV2[1] = IV2[1] + a2;
	    newIV2[2] = IV2[2] + rotate_left(b2,30);
	    newIV2[3] = IV2[3] + rotate_left(c2,30);
	    newIV2[4] = IV2[4] + rotate_left(d2,30);
	  
	    simulate_block(newIV1, newIV2);
	  }
	}
      }
    }
  }

  simulate_block(IV1, IV2);
}
