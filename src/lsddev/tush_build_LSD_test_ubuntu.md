#  build LSD tests - Example & Documentation

This example file contains a set of simple tests to ensure that LSD in window and in no window version can be build.
Exemplarily, the first test `/Test/test_0001_tush` is used here.

For information on how to use tush, see [`developer-setup.md`](developer-setup.md).

## Regression test for LSD no window version

Firstly, we make the specific no window version if it was not already created (i.e. only in single run)

Get the model name which is the name of the temporary dir (not the path, only the dir)
Note: This is test code here. We cannot assign it to a variable here as each $ is a new shell.

$ cd ../Test/test_0001_tush/ && echo $(pwd | grep -oP '^.*\/\K(\w*)')
| test_0001_tush

Secondly, we create the special no window makefile.
Step 1: Copy content from makefile-tush-settings.txt
$ cd ../Test/test_0001_tush/ && cat ../makefile-tush-settings.txt > makefileNW

Step 2: Exchange 'MODEL=' with the dir name / model name
$ cd ../Test/test_0001_tush/ && modelName=$(pwd | grep -oP '^.*\/\K(\w*)') && sed -i s/MODEL=/MODEL=$modelName/g makefileNW

Step 3: Append the file with the content of the makefile-NW.txt
$ cd ../Test/test_0001_tush/ && cat ../../../makefile-NW.txt >> makefileNW

Thirdly, we compile the executable - but do not consider std output of compilation in this test
$ cd ../Test/test_0001_tush/ && if [ -f lsdNW ]; then rm lsdNW; fi
$ cd ../Test/test_0001_tush/ && make -f makefileNW >> tushLogNull.log

Test done for LSD no-window.

## Regression test for regular (LSD Browser -mode) version
TODO not working yet!

Firstly, we make the specific no window version if it was not already created (i.e. only in single run)

Get the model name which is the name of the temporary dir (not the path, only the dir)
Note: This is test code here. We cannot assign it to a variable here as each $ is a new shell.

// $ cd ../Test/test_0001_tush/ && echo $(pwd | grep -oP '^.*\/\K(\w*)')
// | test_0001_tush

Secondly, we create the regular (LSD Browser -mode) makefile.
Step 1: Copy content from makefile-tush-settings.txt and also additonal windowed settings
// $ cd ../Test/test_0001_tush/ && cat ../makefile-tush-settings.txt > makefile
// $ cd ../Test/test_0001_tush/ && cat ../makefile-additional-windowed-settings.txt >> makefile

Step 2: Exchange 'MODEL=' with the dir name / model name
// $ cd ../Test/test_0001_tush/ && modelName=$(pwd | grep -oP '^.*\/\K(\w*)') && sed -i s/MODEL=/MODEL=$modelName/g makefile

Step 3: Append the file with the content of the makefile-linux.txt
// $ cd ../Test/test_0001_tush/ && cat ../../../makefile-linux.txt >> makefile

Thirdly, we compile the executable - but do not consider std output of compilation in this test
// $ cd ../Test/test_0001_tush/ && if [ -f lsd ]; then rm lsd; fi
// $ cd ../Test/test_0001_tush/ && make -f makefile >> tushLogNull.log

Test done for regular (LSD Browser -mode) LSD file.
