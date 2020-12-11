# HORNET

HORNET is a cycle-level on-chip network and multicore simulator. This repository is a copy of the [original Hornet project](http://csg.csail.mit.edu/hornet/), modified only to compile cleanly on recent OS releases.

You can access the user manual and examples at the [original site](http://csg.csail.mit.edu/hornet/), or build it from the sources.


## Supported OS versions

The code compiles and runs on **Ubuntu 18.04** and **Ubuntu 20.04**.

The 20.04 Ubuntu release does not seem to include the `qmtest` test framework, so you will either need to install it by hand or use the `--enable-testsuite=no` option when running `configure`.


## Citations

If you find this useful, please cite:

1. Mieszko Lis, Pengju Ren, Myong Hyon Cho, Keun Sup Shim, Christopher W. Fletcher, Omer Khan, and Srinivas Devadas (2011). Scalable, accurate multicore simulation in the 1000-core era. In _IEEE International Symposium on Performance Analysis of Systems and Software (ISPASS 2011)_.

2. Pengju Ren, Mieszko Lis, Myong Hyon Cho, Keun Sup Shim, Christopher W. Fletcher, Omer Khan, Nanning Zheng, and Srinivas Devadas (2012). HORNET: A Cycle-Level Multicore Simulator. _IEEE Transactions on Computer-Aided Design of Integrated Circuits and Systems (TCAD)_ 31:890–903.

