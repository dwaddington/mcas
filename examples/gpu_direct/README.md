# GPU-Direct Example

This example demonstrates the use of GPU-direct with MCAS xxx_direct APIs to move data from MCAS
directly into and from the GPU memory space.  For the demonstration to work, the following are needed:

1. NVIDIA Tesla
2. CUDA installed
2. nvidia-peer-memory kernel module (https://www.mellanox.com/products/GPUDirect-RDMA)

GPU-direct attributes:
* very low overhead, as it is driven by the CPU. As a reference, currently a cudaMemcpy can incur in a 6-7us overhead.
* an initial memory pinning phase is required, which is potentially expensive, 10us-1ms depending on the buffer size.
* fast H-D (host<--mcas), because of write-combining. H-D bandwidth is 6-8GB/s on Ivy Bridge Xeon but it is subject to NUMA effects.
* slow D-H, (host-->mcas)because the GPU BAR, which backs the mappings, can't be prefetched and so burst reads transactions are not generated through PCIE

For the host-->mcas direction, DRAM buffer-bouncing can be faster.

## Resources

Details about GPU-Direct: https://docs.nvidia.com/cuda/gpudirect-rdma/index.html
CUDA Programming Guide: http://developer.download.nvidia.com/compute/DevZone/docs/html/C/doc/CUDA_C_Programming_Guide.pdf
Notes from gdrcopy: https://github.com/NVIDIA/gdrcopy

## Run

Server side set up MCAS instance, e.g.:

``` bash
DAX_RESET=1 ./dist/bin/mcas --conf ./dist/conf/example-hstore-devdax-0.conf
```

Client:

``` bash
./dist/bin/gpu-direct-demo --server 10.0.0.101:11911
```

## Example output

Our test scenario includes:

* Tesla M60 GPU (Driver Version: 455.32.00)
* Intel Xeon E5-2630 (client)
* Intel Xeon Gold 5218 @2.3GHz with Intel 6x 512GB Optane Persistent Memory Modules (server)
* CUDA Version 11.1
* 2-hops on 100Gbps RNIC across SN2100 switch

Output is:

``` bash
./dist/bin/gpu-direct-demo --debug 3
[LOG]:Tsc: clock frequency 2200.00 mhz 
[LOG]:Buffer_manager 0x2025030 allocated 64 buffers 
NOTICE: TLS is OFF 
[+] >>> Sent handshake (1) 
[+] run_test (cuda app lib) 
[+] There are 8 devices supporting CUDA, picking first... 
[+] [pid = 115637, dev = 0] device name = [Tesla M60] 
[+] creating CUDA Ctx 
[+] making it the current CUDA Ctx 
[+] allocated GPU buffer address at 00000013046e0000 pointer=0x13046e0000 
[LOG]:register_direct_memory (0x13046e0000, 134217728, mr=0x2782420) 
[+] registered memory with MCAS for direct transfers. 
[+] set buffer to 0xBB 
Viewed from GPU: 0xbb 0xbb 0xbb ...
[+] Create pool: gpuPool (flags=0, base=0x0) 
[+] GPU --> MCAS Throughput: 934.306569 MB/s 
[+] Zero'ed GPU memory. 
Viewed from GPU: 0x00 0x00 0x00 ...
[+] About to read back from MCAS... 
[+] GPU <-- MCAS Throughput: 7191.011236 MB/s 
[+] Verifying memory, should be 0xBB-array again... 
Viewed from GPU: 0xbb 0xbb 0xbb ...
[+] Voila! 

```
