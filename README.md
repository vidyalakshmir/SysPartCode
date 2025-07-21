# SyspartCode


This repository contains the source code of "SYSPART: Automated Temporal System Call Filtering for Binaries", published in ACM Conference on Computer and Communications Security (CCS) 2023. (https://dl.acm.org/doi/10.1145/3576915.3623207). The purpose of this repository to run and explore the different functionalities offered by the tool. If you are looking to reproduce the results of the paper, please refer to this repository https://github.com/vidyalakshmir/SysPartArtifact.


We update this repository to add more features and fix bugs. In case of any queries or issues, please contact vrajagop@stevens.edu.


## Upgrades and fixes
We have upgraded SysPart for use in latest ubuntu versions. (ubuntu 18.04+). We have tested in ubuntu 22.04. Please find more info about how to upgrade in the [section](#run-in-latest-oS-versions)

## Basic Requirements
- Works on **linux binaries (ELF)** which run on **x86-64** architecture
- Tested with **ELF binaries with symbols** (application as well as libraries). It will work with stripped binaries, only that the results (like callgraph) might be overapproximated.
- The tool is tested on **Ubuntu 18.04**. Updating to latest linux versions might require some tweaks/changes. (We are working on updating to the latest linux versions. Stay tuned.)


## Capabilities of the tool
### Any application
- Create callgraph of application binaries as well as its dependent libraries
- Get system calls invoked from different functions
- Get system calls reachable from any function as well as from any part of code (instruction)
- Produce targets of indirect calls
- Print address-taken functions
- Resolve the names of libraries and functions loaded using dlopen() and dlsym()
- Resolve arguments passed to functions (for eg: execve())
- Print the noreturn functions
- Print the loops
- Enforces system call filter at a program point


### Server applications
- Determine where initialization phase of the server ends and serving phase begins
- Determine the system calls reachable from the serving phase of the server


## Setup

### Cloning the respository

This repository uses several git submodules for benchmarking. To clone this repository, you __must__ have SSH access configured for [Github](https://github.com). (Please refer this [link](https://docs.github.com/en/authentication/connecting-to-github-with-ssh/adding-a-new-ssh-key-to-your-github-account) for more information.)

Once you ensure your public  keys are configured, you an clone the repository recursively with:

```
git clone --recursive https://github.com/vidyalakshmir/SysPartCode.git
```

#### Run in latest OS versions
The initial repo was tested in ubuntu 18.04. We have upgraded it to work on latest ubuntu versions. As of now, we have tested on ubuntu 22.04. Please issue the following commands for the upgrade
```
cd SysPartCode/analysis/tools/egalito
git checkout egalito-upgrade
```

### Install all dependencies

```
sudo apt-get install make g++ libreadline-dev gdb lsb-release unzip libc6-dbg libstdc++6-7-dbg

sudo apt install libunwind-dev python3
```

### Building the tool

```
./build.sh
```

## Using the tool


### Generate callgraph 
Uses static analysis to generate the callgraph of the application as well as its dependent libraries.

` ./syspart -p $BINARY -s $MAIN -a 1`

where $BINARY is the name of the binary
and $MAIN is the name or address of the function from which the callgraph is to be computed (Usually main()). The address should be provided in hex and should start with 0x.

By default all indirect calls target the AT functions. 
`-i` performs the FCG refining of SysPart (prune AT list and resolve indirect calls).
For more options, refer `./syspart --help`

### Dynamic Library Profiling
The names of libraries and functions loaded using dlopen() and dlsym() are determined by a combination of static and dynamic analyses.


#### Determine the dlopen and dlsym function name

In the environment we tested (Ubuntu 18.04 and libc-2.27), the dlopen and dlsym functions are in libdl.so and its names are `dlopen@@GLIBC_2.2.5` and `dlsym`. Please check and confirm this in the environment that you are using and update `analysis/app/src/dlanalysis/dlopen.txt` with the corresponding dlopen function name and `analysis/app/src/dlanalysis/dlsym.txt` with the corresponding dlsym function name.

#### Static analysis
 ```
 cd analysis/app/src/dlanalysis/static
 ./run_dlanalysis.sh $BINARY $OUTPUT_DIR
 ```
 where $BINARY is the name of the binary and $OUTPUT_DIR is the output folder in which the output files will be stored.
 
 This script runs SysPart's static analysis using value-flow analysis (VFA) and the results are stored in `dlopen_static.txt` and `dlsym_static.txt`. Each line of these files contain the function which invokes dlopen/dlsym along with the callsite address followed by the argument passed to dlopen/dlsym. In case of dlopen(), the first argument, which represents the shared library name is found. In case of dlsym(), the second argument, which represents the function to which a pointer is requested, is found.

 In case where all possible values flowing into dlsym() call sites are resolved, but not to dlopen(), we use heuristics and search the system for libraries exporting any of the resolved symbols. All matching libraries are potential inputs to dlopen() and this list can be seen in `libraries_matching_syms.txt` in the output folder. By default, all libraries found in the paths mentioned in the file `analysis/app/src/dlanalysis/pathlist.txt` are considered to do this search. It is recommended to add application-specific folders to this the file as dependent libraries might reside in those paths.


 #### Dynamic analysis
 For verification purposes, we employ dynamic analysis to get the arguments to dlopen and dlsym seen at runtime.

 ```
 cd analysis/app/src/dlanalysis/dynamic/fninterposition
 make
 LD_PRELOAD=./libmydl.so $APP_RUN_COMMAND
 ```
`APP_RUN_COMMAND` refers to the command to run the application. If the application requires root permissions, run with sudo.
 
 `sudo LD_PRELOAD=./libmydl.so $APP_RUN_COMMAND`

 The results of the run are stored in `output/fninterp_$pid.txt` where `$pid`  is the process id.

 To process the results, run

 `./process_output.sh output/ $OUTPUT_FOLDER`

 This script will produce two files `fninter_dlopen.txt` and `fninterp_dlsym.txt` in the `$OUTPUT_FOLDER` and they will contains the arguments to `dlopen()` and `dlsym()` functions respectively. It also moves `fninterp_$pid.txt` files from `output/` to `$OUTPUT_FOLDER`.


 

### Serving Phase Detection
The serving phase of the server is determined by the combination of static as well as dynamic analysis.

$OUTDIR/ is the location where outputs for each server are stored/

```
mkdir $OUTDIR/pin
```

#### Static analysis to determine all loops

Egalito is used to statically analyze the server binary and all its dependent libraries. The binary is disassembled and control flow graph (CFG) of the binary is extracted. For each function, all loops are identified by employing the worklist algorithm, which builds on the concept of dominance. 

Inorder to find the loops,
```
 cd analysis/app

./loops $BINARY > $OUTDIR/loops_output.out
grep -v '^FUNC' $OUTDIR/loops_output.out > $OUTDIR/loops.out
```

#### Dynamic analysis to determine the dominant loop
This step uses Intel Pin to determine the dominant loop - the top-level loop where the server spends the most amount of time. We develop a pintool that utilizes the data obtained statically in the previous step, including the start address of the loop and the addresses of its exit nodes, to calculate the amount of time each top-level loop encompasses execution for each process and thread.

To run this, you need to know the command to run the server. Let COMMAND_TO_RUN_SERVER be the command to run the server. For example in case of the Bind server, the command to run the server is

```
$BIND_BINARY -f -u bind
```

Now use the following commands to set up the Pin environment.

Let $base_dir be the location of this git repo in your system, then,
```
cd analysis/tools
tar -xvf pin-3.11-97998-g7ecce2dac-gcc-linux.tar.gz
export PIN_ROOT="$base_dir/analysis/tools/pin-3.11-97998-g7ecce2dac-gcc-linux"
```

The following commanD is used to run the pintool. 
```
cd analysis/app/src/pintool

sudo $PIN_ROOT/pin -follow_execv -t obj-intel64/timeofouterloop.so -i $OUTDIR/loops.out -p $OUTDIR/pin/ -o pin.out -- $COMMAND_TO_RUN_SERVER
```

After 30 seconds of running this command, stop the server process. This can be achieved in some cases by issuing Ctrl+C if the command to run the pintool doesn't return, or by using the server specific stop/kill command. For example, in case of Httpd server, the command is, 

```
sudo $HTTPD_BINARY -k graceful-stop
```
Ensuring that the server is stopped is essential to obtain correct output.

Finally, the final results are obtained using the following script
This produces a file $OUTDIR/pin.out with each line corresponding to the serving phase of each of the process/thread of the server. 
```
analysis/app/src/scripts/parse_pinout.sh $OUTDIR/pin
```

### System calls of main() and mainloop


To find the syscalls reachable from a program point with addresss $addr within function $func
```
./syspart -p $BINARY -i -s main -a 2,$addr,$func > syscalls.out

grep 'JSON' syscalls.out | awk {'print $2'} > syscalls.json
grep 'PARTITION_SIZE' syscalls.out | awk {'print $2'} > partition_size.out
grep -w 'MAIN' syscalls.out | awk {'print $2'} > main_syscalls.out
grep -w 'MAINLOOP' syscalls.out | awk {'print $2'} > mainloop_syscalls.out
```
To find the syscalls reachable from a specific function $func,
```
./syspart -p $BINARY -i -s main -a 7,$func
```

To find the syscalls reachable from all functions,
```
./syspart -p $BINARY -i -s main -a 7,*
```

### System Call Enforcement

#### Generate Filter

First step is to generate the syscall filter. The following commands create libsyspart.so which contains the system call filter as a Seccomp-BPF program.

```
cd enforcement/src/scripts
python3 skip_list_filter.py <syscalls_json_file> > ../filter/filter.c
cd ../filter
make
```

#### Insert filter in binary

Transform the binary by inserting the system call filter at the partition boundary.

```
cd enforcement
make
./sysenforce $BINARY <func_name> <partition_addr> $OUTPUT_BINARY sysfilter.so install_filter
```

(`func_name`, `partition_addr`) pairs are from `$OUTDIR/pin.out` and should be the same used to produce the json file containing the allowed system calls, produced by the analysis component.

The command produces a new binary $OUTPUT_BINARY with the system call filter placed (install_filter() function of libsyspart.so is invoked) at the partition boundary.
