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
