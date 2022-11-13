#  Regression tests - Example & Documentation

This example file contains a set of simple tests to ensure that LMM can be build and LSD in no window version can be build and model output is consistent.

For information on how to use tush, see [`developer-setup.md`](developer-setup.md).
## Build LMM test

Note: In the current specification, only the linux test is enabled as this script is written for usage with linux, not windows /macOs

```tush build LMM for Ubuntu
# Clean any old files
$ cd ../.. && make clean -f makefile.LMM >> lsddev/tushLogNull.log
# Build
$ cd ../.. && make -f makefile.LMM >> lsddev/tushLogNull.log
```

## Regression test for LSD no window version
Tests for all existing test models should be included with tests that compile and run the model (using a specific NW setup).
Because one cannot call tush-scripts recursively, the script `macros_regression_test.sh` is called instead. 

Exemplarily, the first test `/Test/test_0001_tush` is also added here:
_Note: The receipe is normaly inside the `/Test/test_0001_tush/description.txt` file and only added here for documentation_

### Preparation
Firstly, we make the specific no window version if it was not already created (i.e. only in single run)

Get the model name which is the name of the temporary dir (not the path, only the dir)
Note: This is test code here. We cannot assign it to a variable here as each $ is a new shell.

```
$ cd ../Test/test_0001_tush/ && echo $(pwd | grep -oP '^.*\/\K(\w*)')
| test_0001_tush
```

#### Create the special no window makefile.
Step 1: Copy content from makefile-tush-settings.txt
```
$ cd ../Test/test_0001_tush/ && cat ../makefile-tush-settings.txt > makefileNW
```

Step 2: Exchange 'MODEL=' with the dir name / model name
```
$ cd ../Test/test_0001_tush/ && modelName=$(pwd | grep -oP '^.*\/\K(\w*)') && sed -i s/MODEL=/MODEL=$modelName/g makefileNW
```

Step 3: Append the file with the content of the makefile-NW.txt
```
$ cd ../Test/test_0001_tush/ && cat ../../../makefile-NW.txt >> makefileNW
```

#### Compile the executable - but do not consider std output of compilation in this test
```
$ cd ../Test/test_0001_tush/ && if [ -f lsdNW ]; then rm lsdNW; fi
$ cd ../Test/test_0001_tush/ && make -f makefileNW >> tushLogNull.log
```

### Run Model
Secondly, we run the tushTest.lsd and expect it to not fail. Important: Pipe stdout to a trash file, to ignore it.
Otherwise you would, e.g., include timing issues.
```
$ cd ../Test/test_0001_tush/ && ./lsdNW -f tushTest.lsd >> tushLogNull.log
```

### Validate Results
Thirdly, we compare the output with the expected output. Here we use tush explicitly.
```
$ cd ../Test/test_0001_tush/ && cat tushTestLog.log
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

Test done.

## Creating regression tests
You could also run the test by navigating to the folder `/Test/test_0001_tush` 
and then calling `sudo tush-check description.txt`. In addition to the above,
there is a switch that check if the executable needs to be build, using fixed 
input for the TCL versions in this case, or if it exists already (see next section).

Any new test should be written like this one, with the ability to be run inside the folder by simply 
calling `sudo tush-check description.txt`. Take the files from the very first test and adjust them.

## Running the regression suite.
Because tush cannot call tush, and also because we may want to update some global variable (like currently the tcl/tk version)
