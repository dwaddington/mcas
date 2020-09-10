# Benchmarking Tool 

MCAS provides a tool for performance testing called `kvstore-perf`.

A basic example of the tool invocation is as follows:

Server-side (example using hstore):

```bash
DAX_RESET=1 ./dist/bin/mcas --conf ./dist/testing/hstore-0.conf
```

Client-side (example):

```bash
./dist/bin/kvstore-perf \
--server 10.0.0.101 \
--port 11911 \
--cores=12,13 \
--component mcas \
--test put \
--key_length 8 \
--value_length 32 \
--element 1000000 \
--debug 0 \
--device_name mlx5_0 
```

This will run a test of 1M insertions (key length 8 and value length 32) on a remote MCAS server. The cores designated (12 and 13) are used for the worker threads.  Each client-side worker thread performs an independent experiment, then the aggregate result is given.  The results from the tool are written into the local subdirectory ./results/xyz where xyz is the name of the component (e.g., mcas).  Alternatively, using the `--skip_json_reporting` can be used to omit report generation and just give console output.



## Local engine test

To continuous measure the 100% read performance (hstore):

```bash
DAX_RESET=1 ./dist/bin/kvstore-perf --cores=0-9 --component hstore --test throughput --read_pct=100 --key_length 8 --value_length 48 --element 100000 --debug 3 --device_name=/dev/dax --size 100000000 --skip_json_reporting
```



#### Parameters for kvstore-perf

```
  --test arg (=all)                     Test name <all|put|get|get_direct|put_direct|throughput|erase|update>. 
                                        Default: put.
                                        
  --component arg (=mcas)               Implementation selection 
                                        <mcas|mapstore|hstore>. Default: mcas.
                                        
  --cores arg (=0)                      Comma-separated ranges of core indexes 
                                        to use for test. A range may be 
                                        specified by a single index, a pair of 
                                        indexes separated by a hyphen, or an 
                                        index followed by a colon followed by a
                                        count of additional indexes. These 
                                        examples all specify cores 2 through 4 
                                        inclusive: '2,3,4', '2-4', '2:3'. 
                                        Default: 0.
                                        
  --devices arg                         Comma-separated ranges of devices to 
                                        use during test. Each identifier is a 
                                        dotted pair of numa zone and index, 
                                        e.g. '1.2'. For compatibility with 
                                        cores, a simple index number is 
                                        accepted and implies numa node 0. These
                                        examples all specify device indexes 2 
                                        through 4 inclusive in numa node 0: 
                                        '2,3,4', '0.2:3'. These examples all 
                                        specify devices 2 thourgh 4 inclusive 
                                        on numa node 1: '1.2,1.3,1.4', 
                                        '1.2-1.4', '1.2:3'.  When using hstore,
                                        the actual dax device names are 
                                        concatenations of the device_name 
                                        option with <node>.<index> values 
                                        specified by this option. In the node 0
                                        example above, with device_name 
                                        /dev/dax, the device paths are 
                                        /dev/dax0.2 through /dev/dax0.4 
                                        inclusive. Default: the value of cores.
                                        
  --path arg (=./data/)                 Path of directory for pool. Default: 
                                        "./data/"
                                        
  --pool_name arg (=Exp.pool)           Prefix name of pool; will append core 
                                        number. Default: "Exp.pool"
                                        
  --size arg (=104857600)               Size of pool. Default: 100MiB.
  
  --flags arg (=2)                      Flags for pool creation. Default: Component::IKVStore::FLAGS_SET_SIZE.
                                        
  --elements arg (=100000)              Number of data elements. Default: 100,000.
                                        
  --key_length arg (=8)                 Key length of data. Default: 8.
  
  --value_length arg (=32)              Value length of data. Default: 32.
  
  --bins arg (=100)                     Number of bins for statistics. Default:
                                        100.
                                        
  --latency_range_min arg (=1e-09)
                                        Lowest latency bin threshold. Default: 
                                        1e-9.
                                        
  --latency_range_max arg (=0.001)      Highest latency bin threshold. Default:
                                        1e-3.
                                        
  --debug_level arg (=0)                Debug level. Default: 0.
  
  --read_pct arg (=0)                   Read percentage in throughput test. 
                                        Default: 0.
                                        
  --insert_erase_pct arg (=0)           Insert/erase percentage in throughput 
                                        test. Default: 0.
                                        
  --owner arg (=owner)                  Owner name for component registration
  
  --server arg (=127.0.0.1)             MCAS server IP address. Default: 
                                        127.0.0.1
                                        
  --port arg (=11911)                   MCAS server port. Default 11911
  
  --port_increment arg                  Port increment every N instances.
  
  --device_name arg (=unused)           Device name.
  
  --pci_addr arg                        PCI address (e.g. 0b:00.0).
  
  --nopin                               Do not pin down worker threads to cores.
  
  --start_time arg                      Delay start time of experiment until 
                                        specified time (HH:MM, 24 hour format 
                                        expected.
                                        
  --verbose                             Verbose output
  
  --summary                             Prints summary statement: most frequent
                                        latency bin info per core
                                        
  --skip_json_reporting                 Disables creation of json report file.
  
  --continuous                          Enables never-ending execution, if 
                                        possible
                                        
  --duration arg                        Throughput test duration, in seconds
  
  --report_interval arg (=5)            Throughput test report interval, in 
                                        seconds. Default: 5
                                        ```
                                        
