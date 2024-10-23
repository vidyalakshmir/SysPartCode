
#### Goals
Syspart was using older version of Egalito. And old egalito was not properly resolving `dlopen` calls in `plt`. Therefore we decided to go with this patch. 
- [x] Build Egalito `metis-main`
- [x] Patch changes of Vidya to `metis-main`
- [x] Build patched Egalito `metis-main`
- [x] Build patched Syspart
---

[I have forked the repository in Gitlab.](https://gitlab.com/fthdrmzzz/egalito/)

---
## Egalito Patch
### Setting up sub-modules and CI image

*These changes are temporary. They will be reverted before merge to main repository*

I have changed the image registry for continuous integration in `.gitlab-ci.yml` to be able to use image in egalito repository in [this commit](https://gitlab.com/fthdrmzzz/egalito/-/commit/36221e7ae1a7d29664307c27b33e18922bc35a93):
```diff
l:8
- DEBIAN_IMAGE: $CI_REGISTRY_IMAGE/test/debian-buster:gtirb
- UBUNTU20_IMAGE: $CI_REGISTRY_IMAGE/test/ubuntu-20.04:gtirb
l:8
+ DEBIAN_IMAGE: registry.gitlab.com/egalito/egalito/test/debian-buster:gtirb
+ UBUNTU20_IMAGE: registry.gitlab.com/egalito/egalito/test/ubuntu-20.04:gtirb
```

I have changed submodule links from relative links to urls to be able to pull them from fork, [commit](https://gitlab.com/fthdrmzzz/egalito/-/commit/a180324961b8418d40744960a05f732eeec9bf56):
```diff
-	url = ../egalito-tests.git
+	url = https://gitlab.com/Egalito/egalito-tests.git 
-	url = ../egalito-docs.git
+	url = https://gitlab.com/Egalito/egalito-docs.git
```
---
### Applying Changes of Vidya

I have simply applied the changes of Vidya on egalito on this branch. I have checked changes hunk by hunk and I didn't notice a conflicting or breaking change. Egalito and Syspart eventually built and worked properly after applying this patch.
```sh
wget https://github.com/columbia/egalito/compare/master...stevens-s3lab:egalito:syspart-updated.diff -O changes_of_vidya.diff
git apply --reject --whitespace=fix ../changes_of_vidya.diff
git add -p
```


---
### Installing Dependencies & Building

In `metis-main`, branch, they have removed capstone and gtirb submodules.  
I found how to install these dependencies in continuous integration scripts. In CI scripts, they have added new method to install dependencies. You can check `get-gtirb.sh` file in [this commit](https://gitlab.com/fthdrmzzz/egalito/-/commit/fd1bc7ee69fc278285090bd86650914f894e1a94):

```diff
l:9
- apt-get install -y libgtirb-dev libgtirb-dbg libgtirb-pprinter ddisasm gtirb-pprinter
+ apt-get install -y libgtirb-dev libgtirb-dbg libcapstone-dev-5.0.0-gt2 libgtirb-pprinter ddisasm gtirb-pprinter
```

I have simply ran this script to retrieve `gtirb` and `capstone`.  
Then I have installed dependencies as it is stated in README except `libstdc++6-7-dbg`. I have installed it as ->`libstdc++6-7dbgsym`.  
After installing the dependencies, It built without a problem and CICD pipeline is fixed.
```sh
sudo apt-get install make g++ libreadline-dev gdb lsb-release unzip
sudo apt-get install libc6-dbg libstdc++6-7-dbg  # names may differ (For Example libstdc++6-7-dbgym in Ubuntu20.04)
# inside egalito workdir
./test/script/get-gtirb.sh
```
---
### Running Tests
in CI scripts, Ubuntu 20 image is tested as following in `.gitlab-ci.yml` file:
```sh
### test-package-template
cd test/example && make -j && cd -
# output: /home/fatih/egalito
./app/build_x86_64/etelf -m test/example/hellocpp hello
# output:Transforming file [test/example/hellocpp]
# Parsing ELF file...
# Performing code generation into [hello]...
./hello 
# Hello, World!


### gtrib-test-ubuntu-20
# This test might need a patch. because for each test case, i found egalito/original-rebuilt-complete. And rebuilt binaries are running without a problem. in test script, existence of original-rebuilt binaries are checked instead of original-rebuilt-complete.
test/gtirb/run_test.sh
Testing ex: 'hello'
#./egalito/original-rebuilt does not exist
Testing ex: 'hi0'
#./egalito/original-rebuilt does not exist
Testing ex: 'islower'
#./egalito/original-rebuilt does not exist
Testing ex: 'getenv'
#./egalito/original-rebuilt does not exist
Testing ex: 'hi5'

### test-template
#    - cd test/codegen && make test && cd -
# In the yaml the following two tests are commented out:
# #   - cd test/unit && ./runner && cd -
# #   - cd test/script && make test && cd -
cd test/codegen && make test && cd -
#./run-tests.sh
#./run-build.sh -m hello
#test passed
#./run-build.sh -m jumptable
#test passed
#./run-system.sh -m /bin/ls
#test passed
#./run-system.sh -m /bin/cat
#test passed
#./run-system.sh -m /bin/gzip
#test passed
#./run-system.sh -m /bin/grep
#test passed
#./run-system.sh -m /usr/bin/env
#test passed
#./run-system.sh -m /usr/bin/make
#test passed
#./run-system.sh -m /usr/bin/dpkg
#test passed
#./run-system.sh -m /usr/bin/find
#test passed
#10 tests passed out of 10
```

***
---

[I forked SysPartCode in github.](https://github.com/fthdrmzzz/SysPartCode/tree/libsec-tool)
## SysPart Patch

### Integrating Egalito
In `.gitmodules` have updated the egalito submodule to make it point to my egalito fork:
```diff
-       url = https://github.com/stevens-s3lab/egalito.git
-       branch = syspart-updated
+       url = git@gitlab.com:fthdrmzzz/egalito.git
+       branch = metis-main-syspart
```

In syspart build process, config directory from Egalito is used. That directory is moved to src in `metis-main`. I updated syspart makefile:
```diff
 $(ALL_OBJECTS): | $(BUILDTREE)
-$(BUILDTREE): ../tools/egalito/config/config.h
+$(BUILDTREE): ../tools/egalito/src/config/config.h
        @mkdir -p $@
 
-../config/config.h:
-       $(call short-make,../tools/egalito/config)
+../src/config/config.h:
+       $(call short-make,../tools/egalito/src/config)
```

I removed the checkout in `build.sh` script because branch is already there in submodules. Do not git clone recursive. Instead, after cloning initiate submodules and update submodules.
```diff
 cd analysis/tools/egalito
 export USE_LOADER=0
-git checkout syspart-updated
 make -j 8
 cd analysis/app
 make
```

### Building

I found that, when we run make -j 8 in egalito working directory, some make src prompts may run before dependency make is complete. Alternative building process (taken from `.gitlab-ci.yml`)
```sh
git clone https://github.com/fthdrmzzz/syspartcode 
cd SysPartCode 
git checkout libsec-tool 
git submodule update --init --recursive

cd analysis/tools/egalito/dep && make && cd -
cd analysis/tools/egalito/src && make -j 8 && cd -
cd analysis/tools/egalito && make all && cd -
cd analysis/app/ && make cd -
```

