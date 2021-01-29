# Benchmark (client-side) using MPI clustering

You must have MPI C installed in your system.  Mellanox OFED bundled
MPI should be OK.  Use 'mpirun' to coordinate benchmark processes
across multiple machines.  You will need to copy SSH keys to all machines
that are being used in the test.

Example invocation:

``` bash
mpirun -np 3 -display-map -hostfile ./hosts -bind-to core ./mcas-mpi-bench --server 10.0.0.101:11911 --test write
```


