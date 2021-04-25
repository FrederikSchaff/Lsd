# This file contains simple tests to ensure that LSD can be build and model output is consistent.

Notes: 
- The temporary top directory the tush script is run from is something like `(...)/lsd/dev/tush-scratch`, so you may want to add `../../` to any path argument to start from the root git directory, or even better, add `cd ../../ && <YOUR COMMANDS>` in front of your commands.
- _Tush_ expects exact output. So you may want to pipe output that is not easily to be specified (like the build output) to null, appending `<YOUR COMMAND>  > tushLogNull.log` to your command, and just expect implicitly that 0 and no stderr is returned. (with the potential of windows line endings, piping to /dev/null is error prone).

## Build tests

Note: In the current specification, only the linux test is enabled as this script is written for usage with linux, not windows /macOs

```build LMM for Ubuntu
$ cd ../../src && make clean -f makefile-linux > tushLogNull.log
$ cd ../../src && make -f makefile-linux > tushLogNull.log
```

## Regression tests
Here, tests for all existing test models should be included with tests that compile and run the model (using a specific NW setup).
#$ cd ../../Test/test_0001_tush && tush-check description.txt


## Performance regression tests
ToDo
