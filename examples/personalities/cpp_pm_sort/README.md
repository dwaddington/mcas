# C++ sorting ADO

The goal of this project is to implement 3 sorting algorithms and compare them in the MCAS framework:
1.  Sort completely in DRAM - used as a baseline to compare DRAM vs PM.
2.  Sort in DRAM with Recovery - acts as a middle ground between using only the DRAM/PM, meant to try and get the best performance in this scenario without sacrificing the persistence desired by using the PM.
3.  Sort in PM - benchmarking the efficiency of the PM itself vs classical RAM flow (Implements recovery to simulate expected usage of PM by it’s users).


## Generating Data for the Test

To work properly after compiling, the test needs to load "chunks" of data from files, which are given as input to the sorting algorithm. The test searches for the files in `BUILD_DIR/examples/personalities/cpp_pm_sort/chunks`.

To generate the chunk files, copy the contents of the folder "chunk_generation" to the folder specified by the `CHUNKS_FOLDER_PATH` macro, and then inside that folder execute:
```
python3 DB_generator.py
```

As 512 chunks (a total of 64G) is generated this step takes a while.
## Running Test (from the build dir)

MCAS server:
```
./dist/bin/mcas --conf ./examples/personalities/cpp_pm_sort/cpp-pm-sort.conf --debug 0 --forced-exit
```

Client:

```
./examples/personalities/cpp_pm_sort/personality-cpp-pm-sort-test --server <server-ip-address> --device <device>  --type <task_num> --reinit <bool> --verify <bool>
```

`--reinit true` means that an existing pool will be deleted if exists, and a new one will be created at the beginning of the test execution.
if specified to be `false`, the same pool will be opened as in the previous execution.

`--type 1`, `--type 2`, `--type 3` means that the sorting task that will run will be 1, 2 or 3 respectively.

`--verify true` will run a verification procedure at the end of the sorting task, verifying that the data is indeed sorted.
if specified to be `false` the verification will not be invoked.

The flags `--type`, `--reinit` and `--verify` are optional. If not specified explicitly, default values will be set to
```
--reinit false --type 3 --verify true
```

It is also possible to change the number of chunks the test is running on. This can be achieved by changing the value of the macro `PM_SORT_NUMBER_OF_CHUNKS`, located at cpp_pm_sort_plugin.h. The number should be a power of two between 2 and 512, with the default being 512.

## Crash Recovery
The flow for crash recovery is as follows, demonstrated for task 3 (for another task set the `--type` flag accordingly):

1. Run the MCAS server:
   ```
    ./dist/bin/mcas --conf ./examples/personalities/cpp_pm_sort/cpp-pm-sort.conf --debug 0 --forced-exit
    ```
   
2. Initially run the test with task 3 specified:
   ```
    ./examples/personalities/cpp_pm_sort/personality-cpp-pm-sort-test --server 127.0.0.1 --device lo  --type 3 --reinit true
    ```

Assume that a crash occurred during execution.

3. Restart the server by running once more
   ```
    ./dist/bin/mcas --conf ./examples/personalities/cpp_pm_sort/cpp-pm-sort.conf --debug 0 --forced-exit
   ```
    
4. Run the test again, without `reinit` flag (implicitly setting it to `false`):
   ```
    ./examples/personalities/cpp_pm_sort/personality-cpp-pm-sort-test --server 127.0.0.1 --device lo  --type 3
    ```
   
This will resume the sorting procedure from the relevant stage, according to the specified task and to the place where the crash occurred.

When recovering from a crash, the same task as the one in which the process crashed must be specified.
To run a different task, use  `--reinit true`.
