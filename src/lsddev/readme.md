# LSD Developer Readme

LSD is an open-source framework and thus can be used and also extended by everyone. The "official" version is hosted at github under https://github.com/marcov64/Lsd and at the moment maintained mainly by Marcelo Pereira, University of Campinas. The documents and files residing inside this subfolder `/src/lsddev/` are currently maintained mainly by Frederik Schaff (https://github.com/FrederikSchaff) and shall enable other persons to contribute to LSD, while ensuring that a) such contributions would not break anything existing, b) such contributions would fit within the current (grown...) architecture of LSD and, not unimportant, c) evaluating such new contributions is possible in an efficient way, respecting the little resources available for such issues.

LSD uses a DSL (domain specific language) implemented via c preprocessor macros. All core LSD DSL macros reside in [`src/fun_head.h`](../fun_head.h). Any new user facing functionality should follow the standards implicitly provided by these macros, but have their own header file (and source files). 

It is planned to cover, step-by-step, all existing macros in special test models. These test modles are automatically compiled and executed with simple tush scripts (see below). This ensures that a) it is easy for a reviewer to understand, from a user perspective, what those macros accomplish and b) the macros have been tested extensively. This is an additional requirement to a user facing documentation and user facing example models / tutorials.

A guide and example for "plugin" extensions to LSD will follow in future.

See [developer-setup](developer-setup.md) for detailed instructions on how to setup a developer environment with artistic style (formatting) and tush (DSL testing).

## Testing

### Tush shell scripted testing
With [`tush_local_simple_regression_test.md`](tush_local_simple_regression_test.md) a small number of 'tests' are implemented to ensure that
- LMM compiles (LMM: LSD Model Manager - the IDE that allows to manage LSD models and develop the code)
- LSD headless version ("no window") compiles and executes models correctly
- LSD window version compiles and starts correctly
- *Note: This is not yet complete!*

With [`macros_regression_test.sh`](macros_regression_test.sh) a simple shell script is implemented which iterates over all test models contained in [`src/ldddev/Test`](Test/), executing them and reporting any test that failed.

## Github workflows
Github workflows (see [.github/workflows](../../.github/workflows/) ) are implemented to allow:
- building of LMM for all major platforms (to do)
- running the [`tush_local_simple_regression_test.md`](tush_local_simple_regression_test.md)
- running all macro tests by running [`macros_regression_test.sh`](macros_regression_test.sh)

It is planned to implement automated workflows for all major platforms. UI testing is currently not planned, as it is more complex, there are less changes expected and the correctness of model runs does not depend on the UI.

For now all workflows require manual actions and run on the main branch of the repository. In future we may change this.

### Status of github workflows:

LMM Builds:
[![LMM build (ubuntu)](https://github.com/FrederikSchaff/Lsd/actions/workflows/LMM_build_ubuntu.yml/badge.svg)](https://github.com/FrederikSchaff/Lsd/actions/workflows/LMM_build_ubuntu.yml) 

Test workflows:
[![Test LSD build and macros (manually)](https://github.com/FrederikSchaff/Lsd/actions/workflows/manual-full-test.yml/badge.svg)](https://github.com/FrederikSchaff/Lsd/actions/workflows/manual-full-test.yml)