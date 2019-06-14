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

  struct diff_group {
    int z;
    struct {double p; uint32_t diff[5];} g[38];
  };

  struct diff_group diff_groups[] = {
#include "diff_groups.h"
  };

int main() {

  srand(getpid()*0xdeadbeef + time(NULL)*0xbadc0ffe);
  
  int n = random()%tablength(diff_groups);

  printf ("Checking group %i (z=0b", n);
  for (int i=7; i>=0; i--)
    printf ("%i", (diff_groups[n].z>>i)&1);
  printf (")\n");
  int z = ~diff_groups[n].z;
  printf ("  Z conditions: z13=%i", ((z<<=1)>>13)&1);
  printf (" z12=%i", ((z<<=1)>>13)&1);
  printf (" z11=%i", ((z<<=1)>>13)&1);
  printf (" z10=%i", ((z<<=1)>>13)&1);
  printf (" z9=%i", ((z<<=1)>>13)&1);
  printf (" z1=%i", ((z<<=1)>>13)&1);
  printf (" z7=%i", ((z<<=1)>>13)&1);
  printf (" z2=%i", ((z<<=1)>>13)&1);
  printf (" z3=%i", ((z<<=1)>>13)&1);
  printf (" z8=%i", ((z<<=1)>>13)&1);
  printf (" z6=%i", ((z<<=1)>>13)&1);
  printf (" z4=%i", ((z<<=1)>>13)&1);
  printf (" z5=%i", ((z<<=1)>>13)&1);
  printf ("\n");


  
  uint64_t count[tablength(diff_groups[n].g)] = {0};
  
#define SAMPLE (1ULL<<28)
  printf ("Using 2^%i samples\n", (int)log2(SAMPLE));
  
  for (uint64_t t = 0; t < SAMPLE; t++) {
    // Random state at step 64
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

    // Random message
    uint32_t m[16]; // Last 16 message words
    for (int i=0; i<16; i++)
      m[i] = (rand()<<16) + rand();

#define setm(i,j,z) m[(i)-64] &= ~(1U<<(j)), m[(i)-64] |= (z)<<(j)

    // Apply zi conditions
    int z = ~diff_groups[n].z;
    setm(79, 4, ((z<<=1)>>13)&1); // z13
    setm(79, 2, ((z<<=1)>>13)&1); // z12
    setm(78, 7, ((z<<=1)>>13)&1); // z11
    setm(78, 3, ((z<<=1)>>13)&1); // z10
    setm(78, 0, ((z<<=1)>>13)&1); // z9
    setm(73, 2, ((z<<=1)>>13)&1); // z1
    setm(76, 3, ((z<<=1)>>13)&1); // z7
    setm(76, 1, ((z<<=1)>>13)&1); // z2
    setm(76, 0, ((z<<=1)>>13)&1); // z3
    setm(77, 8, ((z<<=1)>>13)&1); // z8
    setm(77, 2, ((z<<=1)>>13)&1); // z6
    setm(77, 1, ((z<<=1)>>13)&1); // z4
    setm(77, 0, ((z<<=1)>>13)&1); // z5

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

    // Apply message difference corresponding to path
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

    // Compute output difference
    uint32_t dd = rotate_left(d2,30) - rotate_left(d1,30);
    uint32_t dc = rotate_left(c2,30) - rotate_left(c1,30);
    uint32_t db = rotate_left(b2,30) - rotate_left(b1,30);
    uint32_t da = a2 - a1;
    uint32_t de = e2 - e1;

    for (unsigned i=0; i < tablength(diff_groups[n].g) && diff_groups[n].g[i].p; i++) {
      if (diff_groups[n].g[i].diff[0] == de &&
	  diff_groups[n].g[i].diff[1] == da &&
	  diff_groups[n].g[i].diff[2] == db &&
	  diff_groups[n].g[i].diff[3] == dc &&
	  diff_groups[n].g[i].diff[4] == dd) {
	count[i]++;
      }
    }
  }

  // Print results
  for (unsigned i=0; i<tablength(diff_groups[n].g) && diff_groups[n].g[i].p; i++) {
    printf ("Diff [%08x %08x %08x %08x %08x]\t\tpredicted p=2^-%f\t\tmeasured p=2^%f\n",
	    diff_groups[n].g[i].diff[0], diff_groups[n].g[i].diff[1], diff_groups[n].g[i].diff[2],
	    diff_groups[n].g[i].diff[3], diff_groups[n].g[i].diff[4],
	    diff_groups[n].g[i].p, log2((double)count[i]/SAMPLE));
  }
    
}
  
