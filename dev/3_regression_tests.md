# This file contains simple tests to ensure that LSD can be build and model output is consistent.

Notes: 
- The temporary top directory the tush script is run from is something like `(...)/lsd/dev/tush-scratch`, so you may want to add `../../` to any path argument to start from the root git directory, or even better, add `cd ../../ && <YOUR COMMANDS>` in front of your commands.
- _Tush_ expects exact output. So you may want to pipe output that is not easily to be specified (like the build output) to null, appending `<YOUR COMMAND>  > /dev/null` to your command, and just expect implicitly that 0 and no stderr is returned.

## Build tests

Note: In the current specification, only the linux test is enabled as this script is written for usage with linux, not windows /macOs

```build LMM for Ubuntu
$ cd ../../src && make clean -f makefile-linux > /dev/null
$ cd ../../src && make -f makefile-linux > /dev/null
```


## Regression tests
Here, tests for all existing example models should be included for compilation (both, NW and regular) and running the defined file (NW).

### Examples/Test section
ToDo
- extract script to create makefile with default linux options
- add test.tush.md file in folder(s) which will create the makefile, build the window and no window version, run the no window version with a test.tush.sim file and (later) also checks the output against an expected.tush.csv/res output using a simple binary diff.


## Performance regression tests
ToDo
