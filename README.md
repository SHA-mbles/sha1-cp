# SHA1 Chosen-prefix Collision Attack

This repository contains additional data for the paper:

> *From Collisions to Chosen-Prefix Collisions — Application to Full SHA-1*  
> Gaëtan Leurent and Thomas Peyrin  
> Eurocrypt 2019

The paper has been published at Eurocrypt 2019, and a preprint is
available at https://eprint.iacr.org/2019/459.


## Repository content

This repository includes:

1. A full description of the **set of output differences** 𝓓 and the
   bundles used in the attack.  This was obtained by sampling the last
   rounds of SHA-1, and extends the partial data given in
   Table 4 of the paper.  
   The set is described in file `diff_group.h`, and the program
   `sampling.c` can be used to verify the sampling.

2. A **simulator** for the attack, to enable easy verification of our
   claims (see below for details).

3. A **graph** for the attack, corresponding to the set 𝓢
   with maximum cost 2×C_block_ (first line of Table 5).  
   Due to size limits, the graph is not in the git repo, but if available as part of the releases:  
   https://github.com/Cryptosaurus/sha1-cp/releases/download/v1/diffset  
   (Note: the graph only contains the nodes, the edges are recomputed on the fly)

## Attack simulator

### What the simulator does

For each block, our simulator computes the message equations and list
of useful output differences from the graph 𝓖
and simulates the attack by picking random messages and internal
states at step 64 until reaching a useful output difference.  This
validates both the overall attack strategy, and the sampling results;
the simulation results closely match the claims in the paper.

### How to use it

To compile the program, just run `make`.  To use the simulator, you
need a graph for the attack; you can download one with a release of
the code: https://github.com/Cryptosaurus/sha1-cp/releases/download/v1/diffset

There are several ways to use the attack simulator, here are a few example:

1. Generate conditions and path template for a given difference
```
./simulator -sdiffset -dffffda04/fffffed4/fffffffc/fffffff8/00000000 -ttemplate
```

2. Simulate attack form a random input difference in the set
```
./simulator -sdiffset -b
```

## CC0 Public Domain Dedication

To the extent possible under law, the author(s) have dedicated all
copyright and related and neighboring rights to this software to the
public domain worldwide. This software is distributed without any
warranty.  You should have received a copy of the CC0 Public Domain
Dedication along with this software. If not, see
http://creativecommons.org/publicdomain/zero/1.0/.


## Contact

If you have any questions, feel free to contact us:

- Gaëtan Leurent: gaetan.leurent@inria.fr
- Thomas Peyrin: thomas.peyrin@ntu.edu.sg
