# LSD(-GIS) developer setup guide

In this (hopefully) living documentation, some hints and useful tools for the development of LSD are noted down.

It is presumed that you are developing LSD with a Linux machine. However, all the comments should also be useful when configuring for another development system (native Windows, MacOs)

## Code Formatting
- Copy the `pre-commit` file in this folder into your `LSD/.git/hooks` folder. Thus, before committing, it will be checked that the changed/added 
files comply with the formatting rules defined in `LSD/src/.astylerc`. No formatting will be done by the pre-commit hook.
- Install the latest version of [Artistic Style](http://astyle.sourceforge.net/) and make sure that it can be called from the command line.
- Integrate artistic style into your source code editor for automatic formatting. For VS Code, see e.g. [VS Code extension](https://marketplace.visualstudio.com/items?itemName=chiehyu.vscode-astyle). Alternatively you can also run astyle manually on all changed files prior to the commit, the pre-commit hook has the basis for a script that runs on all changed files (just remove the dry-run option and call it from the git top directory). You can also use astyle directly and rely on its usage of the `src/.astylerc` file, which will be used automatically / can be configured. 
For example, assuming your current working directory is the top LSD (git) directory, this command formats the gis.cpp file according to the formatting rules provided in src/.astylerc:
```shell
astyle --project=.astylerc src/gis.cpp
```
## Simple automated testing
In order to provide a very simple test automation, we make use of [tush](https://github.com/darius/tush). A simple introduction is given in [this post](https://spin.atomicobject.com/2016/01/11/command-line-interface-testing-tools/). 

### Installation
Download tush somewhere, say `/opt`. Then create simlinks to your path.
```shell
cd /opt/tush
git clone https://github.com/darius/tush.git
sudo ln -s /opt/tush/bin/overwrite /usr/bin/overwrite
sudo ln -s /opt/tush/bin/tush-check /usr/bin/tush-check
sudo ln -s /opt/tush/bin/tush-bless /usr/bin/tush-run-bless
sudo ln -s /opt/tush/bin/tush-run /usr/bin/tush-run
sudo ln -s /opt/tush/bin/tush-run-raw /usr/bin/tush-run-raw
```

### Example
This will enable you use tush on a simple text file with shell commands you want tush to execute and "test". Tush commands look as follows:

```example.txt
$ echo Hello world #the command
| Hello world
```

The expected stdout here is `Hello world`

```example2.txt
$ doesNotExist #a command which will not work
@ sh: 1: doesNotExist: not found
? 127
```

The expected stderr here is `doesNotExist: command not found` and the return value expected is `127`. (At least on ubuntu 20.04)

Now, try tush by running it on this file like `tush-check 1_developer-setup.md`. It should fail (error code) and provide you with a diff of what failed. It will fail because of the following:

```example3.txt
$ echo Hello you there! #the command
| Hello world
```
Because the expected output is not equal to the expected input. Now, run the other useful command to correct this (but do not forget to revert this change latter...): `tush-bless 1_developer_setup.md` which will exchange the expected output with the actual output and then run `tush-check 1_developer-setup.md` again to see that it actually succeeds now.

### Expected usage
In this folder, you will find a file called `3_regression_tests.md`, in which a set of regression tests for compiling the project (build), compiling the example models (build) and running predefined simulations with the example models (test) will be set-up. This does not provide a coverage report or allow to detect a failure rate, but it does allow to detect much better than now **IF** something fails and also pinpoint **WHERE** it fails.
