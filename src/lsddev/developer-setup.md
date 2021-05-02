# LSD developer setup guide

In this (hopefully) living documentation, some hints and useful tools for the development of LSD are noted down.

It is presumed that you are developing LSD with a Linux machine (or similar, e.g. Windows with WSL2). However, all the comments should also be useful when configuring for another development system (native Windows, MacOs).

Please note that all receipes, suggestions, comments, i.e. everything below, is provided WITHOUT ANY WARRANTY WHATSOEVER! Make sure you understand what you do and also check by yourself that this will not break anything.

## Code Formatting (only for newly added files)
- Copy the `pre-commit` file in this folder into your `LSD/.git/hooks` folder. Thus, before committing, it will be checked that the changed/added 
files comply with the formatting rules defined in `LSD/src/.astylerc`. No formatting will be done by the pre-commit hook.
- Install the latest version of [Artistic Style](http://astyle.sourceforge.net/) and make sure that it can be called from the command line.
- Either integrate artistic style into your source code editor for automatic formatting. For VS Code, see e.g. [VS Code extension](https://marketplace.visualstudio.com/items?itemName=chiehyu.vscode-astyle). 
- Alternatively you can also run astyle manually on all changed files prior to the commit, the pre-commit hook has the basis for a script that runs on all changed files residing inside `src/lsddev` and below (just remove the dry-run option and call it from the git top directory). You can also use astyle directly and rely on its usage of the `src/.astylerc` file, which will be used automatically / can be configured. 

For example, assuming your current working directory is the top LSD (git) directory, this command formats the gis.cpp file according to the formatting rules provided in src/.astylerc:
```shell
astyle --project=.astylerc src/gis.cpp
```
## Simple automated testing with Tush
In order to provide a very simple test automation, we make use of [tush](https://github.com/darius/tush). A simple introduction is given in [this post](https://spin.atomicobject.com/2016/01/11/command-line-interface-testing-tools/). 
Some changes (linting and colored diff, as of writing this) have been added to the [version of tush we use](https://github.com/FrederikSchaff/tush), this version is available directly here in the `LSD/dev/tush` directory.

### TL;DR
Basically, *tush* allows to run any command from the shell, capture the outputs (stdout, stderr, return value) and compares it against the defined output in the document. The tush commands itself are documented inside a text file, together with the expected return, which is used as input to the `tush-check` command. Updating the file (i.e. using the effective return to define the expected return) can be accomplished by runing the file with `tush-bless`. 
We use tush to test building LSD models and LMM (currently only for Linux), currently focussing on the no-window version of LSD. To provide tests for LSD Macros, models that use the subsets of the Macros are created and saved in the `LSD/Test` work folder.
A simple utility header only library for testing `LSD/Test/tush_test_utilities.h` introduces a new LSD Macro `TEST_UNCHANGED(logentry)` which can be used within a regular LSD model to write stuff into a simple, predefined, logfile. The content of this logfile is then tested against the expected content (simply using `cat` to print it). Thus, the output can be completely controlled by the test model. The description of the model contains, besides the description of what the test does, the commands to build & run the model and check the model output. For an example see `LSD/Test/test_0001_tush`.

### Installation to own dev environment
Simplest: Create simlinks to the version inside your local clone of this repository.
```bash
sudo ln -s <LSD>/src/lsddev/tush/bin/overwrite /usr/bin/overwrite
sudo ln -s <LSD>/src/lsddev/bin/tush-check /usr/bin/tush-check
sudo ln -s <LSD>/src/lsddev/bin/tush-bless /usr/bin/tush-run-bless
sudo ln -s <LSD>/src/lsddev/bin/tush-run /usr/bin/tush-run
sudo ln -s <LSD>/src/lsddev/bin/tush-run-raw /usr/bin/tush-run-raw
```
where `<LSD>` is your local repository path.

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

Now, try tush by running it on this file like `tush-check developer-setup.md`. It should fail (error code) and provide you with a diff of what failed. It will fail because of the following:

```example3.txt
$ echo Hello you there! #the command
| Hello world
```
Because the expected output is not equal to the expected input. Now, run the other useful command to correct this (but do not forget to revert this change latter...): `tush-bless developer_setup.md` which will exchange the expected output with the actual output and then run `tush-check developer-setup.md` again to see that it actually succeeds now.

### Expected usage
See the first description file for the first test model [Test/test_0001_tush](Test/test_0001_tush/description.txt) as an example. Basically, it is expected that you create a set of test models that covers all newly integrated macros by printing to a specific log, using a macro defined in the header only [`Test/tush_test_utilities.h`](Test/tush_test_utilities.h), `TEST_UNCHANGED(logentry)`. The expected output is provided in the model description file, which is also the tush script and automatically picked up by the [`macros_regression_test.sh`](macros_regression_test.sh) script. The log entries should explain in plain english what the model is expected to do and what it really does, so that any 'error' (i.e. actual log entry deviating from the predefined expectation) can be understood by the user. In addition, just reading the expected output in the description should allow a reviewer to understand the test and also evaluate whether the test is sufficiently comprehensive.

## Continous Integration with GitHub Actions
In Progress:
It is planned to provide a github action which runs all the tests automatically.

### Locally running the actions
To make use of this setup and run all actions locally, you may make use of [nektos/act](https://github.com/nektos/act) framework.

**Currently, act does not pick up the workflows on my machine, though.**

Prerequisits (Ubuntu@WSL2):
- [NixOs](https://nixos.org/download.html) installed
- use NixOs to install act to your Ubuntu: `nix-env -iA nixpkgs.act` (from [netkos/act github](https://github.com/nektos/act) )
- run `act -l` to see all actions
- run `act` to run the default push action.

