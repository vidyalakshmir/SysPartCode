# SyspartCode


This repository contains the source code of "SYSPART: Automated Temporal System Call Filtering for Binaries", published in ACM Conference on Computer and Communications Security (CCS) 2023. (https://dl.acm.org/doi/10.1145/3576915.3623207). We update this repository to add more features and fix bugs.


## Basic Requirements
- Works on **linux binaries (ELF)** which run on **x86-64** architecture
- Tested with **ELF binaries with symbols** (application as well as libraries). It will work with stripped binaries, only that the results (like callgraph) might be overapproximated.
- The tool is tested on **Ubuntu 18.04**. Updating to latest linux versions might require some tweaks/changes


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

###Dynamic Library Profiling
The names of libraries and functions loaded using dlopen() and dlsym() are determined by a combination of static and dynamic libraries.


##### Determine the dlopen and dlsym function name

In the environment we tested (Ubuntu 18.04 and libc-2.27), the dlopen() and dlsym() is in libdl.so and its names are dlopen@@GLIBC_2.2.5 and dlsym. Please check and confirm this in the environment that you are using. And update `analysis/app/src/dlanalysis/dlopen.txt` with the corresponding dlopen function name and `analysis/app/src/dlanalysis/dlsym.txt` with the corresponding dlsym function name.

##### Static analysis
 `
 cd analysis/app/src/dlanalysis/static
 ./run_dlanalysis.sh $BINARY $OUTPUT_DIR
 `
 where $BINARY is the name of the binary and $OUTPUT_DIR is the output folder in which the output files will be stored.
 
 This script runs SysPart's static analysis using value-flow analysis (VFA) and the results are stored in `dlopen_static.txt` and `dlsym_static.txt`. Each line of these files contain the function which invokes dlopen/dlsym along with the callsite address followed by the argument passed to dlopen/dlsym. In case of dlopen(), the first argument, which represents the shared library name is found. In case of dlsym(), the second argument, which represents the function to which a pointer is requested, is found.

 In case where all possible values flowing into dlsym() call sites are resolved, but not to dlopen(), we use heuristics and search the system for libraries exporting any of the resolved symbols. All matching libraries are potential inputs to dlopen() and this list can be seen in `libraries_matching_syms.txt` in the output folder. By default, all libraries found in the paths mentioned in the file `analysis/app/src/dlanalysis/pathlist.txt` are considered to do this search. It is recommended to add application-specific folders to this path as dependent libraries might reside in those paths.


 ##### Dynamic analysis
 For verification purposes, we employ dynamic analysis to get the arguments to dlopen and dlsym seen at runtime.

 `
 cd analysis/app/src/dlanalysis/dynamic/fninterposition
 make
 LD_PRELOAD=./libmydl.so $APP_RUN_COMMAND
 `
`APP_RUN_COMMAND` refers to the command to run the application. If the application requires root permissions, run with sudo.
 
 `sudo LD_PRELOAD=./libmydl.so $APP_RUN_COMMAND`

 The results of the run are stored as `output/fninterp_$pid.txt` where `$pid`  is the process id.

 To process the results of the file, run

 `./process_output.sh output/ $OUTPUT_FOLDER`

 This script will produce two files `fninter_dlopen.txt` and `fninterp_dlsym.txt` in the `$OUTPUT_FOLDER` which contains the arguments to `dlopen()` and `dlsym()` functions. It also moves `fninterp_$pid.txt` files from `output/` to `$OUTPUT_FOLDER`.


 `

#Serving Phase Detection

#System calls of main() and mainloop


#System Call Enforcement