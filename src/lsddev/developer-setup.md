# LSD developer setup guide

In this (hopefully) living documentation, some hints and useful tools for the development of LSD are noted down. It is not complete nor necessary completely correct but, hopefully, still helpful.

It is presumed that you are developing LSD with a Linux machine (or similar, e.g. Windows with WSL2). However, all the comments should also be useful when configuring for another development system (native Windows, MacOs).

It is also assumed that you followed the setup instructions in the main [LSD/Readme.txt](../../Readme.txt) for linux.

Please note that all receipes, suggestions, comments, i.e. everything below, is provided WITHOUT ANY WARRANTY WHATSOEVER! Make sure you understand what you do and also check by yourself that this will not break anything.

## Code Formatting (only for newly added files)
- Copy the `pre-commit` file in this folder into your `LSD/.git/hooks` folder. Thus, before committing, it will be checked that the changed/added 
files in the `src/lsddev` folder comply with the formatting rules defined in `LSD/src/.astylerc`. No formatting will be done by the pre-commit hook.
- Install the latest version of [Artistic Style](http://astyle.sourceforge.net/) and make sure that it can be called from the command line.
- Either integrate artistic style into your source code editor for automatic formatting. For VS Code, see e.g. [VS Code extension](https://marketplace.visualstudio.com/items?itemName=chiehyu.vscode-astyle). 
- Or, alternatively you can also run astyle manually on all changed files prior to the commit, the pre-commit hook has the basis for a script that runs on all changed files residing inside `src/lsddev` and below (just remove the dry-run option and call it from the git top directory). You can also use astyle directly and rely on its usage of the `src/.astylerc` file, which will be used automatically / can be configured. 

For example, assuming your current working directory is the top LSD (git) directory, this command formats the gis.cpp file according to the formatting rules provided in src/.astylerc:
```shell
astyle --project=.astylerc src/gis.cpp
```
## Simple automated testing with Tush
In order to provide a very simple test automation, we make use of [tush](https://github.com/darius/tush). A simple introduction is given in [this post](https://spin.atomicobject.com/2016/01/11/command-line-interface-testing-tools/). 
Some changes (linting and colored diff, as of writing this) have been added to the [version of tush we use](https://github.com/FrederikSchaff/tush), this version is available directly here in the [`LSD/src/lsddev/tush`](tush) directory.

### TL;DR
Basically, *tush* allows to run any command from the shell, capture the outputs (stdout, stderr, return value) and compares it against the defined output in the document. The tush commands itself are documented inside a text file, together with the expected return, which is used as input to the `tush-check` command. Updating the file (i.e. using the effective return to define the expected return) can be accomplished by runing the file with `tush-bless`. 

We use tush to test building LSD models and LMM (currently only for Linux), currently focussing on the no-window version of LSD. To provide tests for LSD Macros, models that use the subsets of the Macros are created and saved in the [`LSD/src/lsddev/Test`](Test) folder.

A simple utility header only library for testing [`LSD/lsddev/Test/tush_test_utilities.h`](Test/tush_test_utilities.h) introduces a new LSD Macro `TEST_UNCHANGED(logentry)` which can be used within a regular LSD model to write stuff into a simple, predefined, logfile. The content of this logfile is then tested against the expected content (simply using `cat` to print it). Thus, the output can be completely controlled by the test model. The description of the model contains, besides the description of what the test does, the commands to build & run the model and check the model output. For an example see [`LSD/src/lsddev/Test/test_0001_tush`](Test/test_0001_tush) and the [description.txt](Test/test_0001_tush/description.txt) inside which contains the tush script. By convention, the model file should follow the naming of the model directory. The makefile uses this information in its receipe. The script to run all test models makes use of the convention that all test models reside in the [`LSD/src/lsddev/Test`](Test) directory and contain a `description.txt` with the tush script. By

To run and test such a model as regular LSD model just copy the folder into the working directory and ensure to either comment out the use of the **`TEST_UNCHANGED(logentry)`** makro or copy the [`LSD/lsddev/Test/tush_test_utilities.h`](Test/tush_test_utilities.h) into the folder where the model directory resides. 

Additional notes: 
- The temporary top directory the tush script is run from is something like `(...)/lsd/dev/tush-scratch`, so you may want to add `../../` to any path argument to start from the root git directory, or even better, add `cd ../../ && <YOUR COMMANDS>` in front of your commands.
- _Tush_ expects exact output. So you may want to pipe output that is not easily to be specified (like the build output) to null, appending `<YOUR COMMAND>  > tushLogNull.log` to your command, and just expect implicitly that 0 and no stderr is returned. (with the potential of windows line endings, piping to /dev/null is error prone).

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

Now, try tush by running it on this file, opening a terminal in the parent directory of this file and executing `tush-check developer-setup.md`. It should fail (`echo $?` returns `1`) and provide you with a coloured diff of what failed. It will fail because of the following snippet:

```example3.txt
$ echo Hello you there! #the command
| Hello world
```
Because the expected output is not equal to the expected input. Now, run the other useful command to correct this (but do not forget to revert this change latter...): `tush-bless developer_setup.md` which will exchange the expected output with the actual output and then run `tush-check developer-setup.md` again to see that it actually succeeds now. *Do not forget to undo these changes before committing anything*

## Optional: Linking Test folder for use with LMM
As an optional step, you can link the [Test](Test/) folder herin to the main LSD folder, thus allowing to excess is via LMM to setup/inspect the models in the native LSD workflow and also running graphical versions of the models and making use of the autogenerated makefiles.

(Exchange <LSD> with your path to the LSD root folder)

On linux you can link the folder as follows:
```shell
cd <LSD> #navigate to LSD Repo directory
ln -s src/lsddev/Test Test #Link the folder
```
_Note that a folder linked via WSL shell on Windows cannot be accessed from Windows. You may link the directory in both, Windows and WSL though - but with different names!_

On Windows you can link the folder as follows (needs a power shell terminal with admin privilidges)
```power shell (admin)
New-Item -ItemType Junction -Path <LSD>/TestW -Target "<LSD>/src/lsddev/Test"
```



## Continous Integration with GitHub Actions
The github action [`complete_user_macro_regression.yml`](LSD/.github/workflows/complete_user_macro_regression.yml) runs the regression script and updates the status:
[![LSD user macros complete regression test](https://github.com/FrederikSchaff/Lsd/actions/workflows/complete_user_macro_regression.yml/badge.svg)](https://github.com/FrederikSchaff/Lsd/actions/workflows/complete_user_macro_regression.yml)

### Locally running the actions
To make use of this setup and run all actions locally, you may make use of [nektos/act](https://github.com/nektos/act) framework.

**Currently, act does not pick up the workflows on my machine, though.**

Prerequisits (Ubuntu@WSL2):
- [NixOs](https://nixos.org/download.html) installed
- use NixOs to install act to your Ubuntu: `nix-env -iA nixpkgs.act` (from [netkos/act github](https://github.com/nektos/act) )
- run `act -l` to see all actions
- run `act` to run the default push action.

