#  Regression tests - Example & Documentation

This example file contains a set of simple tests to ensure that LMM can be build and LSD in no window version can be build and model output is consistent.

For information on how to use tush, see [`developer-setup.md`](developer-setup.md).
## Build LMM test

Note: In the current specification, only the linux test is enabled as this script is written for usage with linux, not windows /macOs

```tush build LMM for Ubuntu
# Clean any old files
$ cd ../.. && make clean -f makefile-linux >> lsddev/tushLogNull.log
# Build
$ cd ../.. && make -f makefile-linux >> lsddev/tushLogNull.log
```

## Regression test for LSD no window version
Tests for all existing test models should be included with tests that compile and run the model (using a specific NW setup).
Because one cannot call tush-scripts recursively, the script `macros_regression_test.sh` is called instead. 

Exemplarily, the first test `/Test/test_0001_tush` is also added here:

```tush build lsd no window version and run it
#Clean any old files
$ cd ../Test && make clean -f makefileNWtush MODEL=test_0001_tush  >> ../tushLogNull.log
# build executable
$ cd ../Test && make -f makefileNWtush MODEL=test_0001_tush >> ../tushLogNull.log

# run model, do not use the output for testing (because, e.g., timing and other output is not controlled)
$ cd ../Test/test_0001_tush && ./lsdTush -f tushTest.lsd >> ../../tushLogNull.log

# parse the controlled output from the log file and compare it to the expected output written below
$ cd ../Test/test_0001_tush && cat tushTestLog.log
| Begin of test logs for 'test_0001_tush'
| 
| 
| Test log. Step: 1.
| Test log. Step: 2.
| Test log. Step: 3.
| 
| 
| End of test logs for 'test_0001_tush'

```

## Creating regression tests
You could also run the test by navigating to the folder `/Test/test_0001_tush` 
and then calling `sudo tush-check description.txt`. In addition to the above,
there is a switch that check if the executable needs to be build, using fixed 
input for the TCL versions in this case, or if it exists already (see next section).

Any new test should be written like this one, with the ability to be run inside the folder by simply 
calling `sudo tush-check description.txt`. Take the files from the very first test and adjust them.

## Running the regression suite.
Because tush cannot call tush, and also because we may want to update some global variable (like currently the tcl/tk version)
