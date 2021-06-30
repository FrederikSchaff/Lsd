# Gis integration project

See [Readme.txt](Readme.txt) for details on LSD itself.

- [Gis integration project](#gis-integration-project)
  - [TL;DR](#tldr)
  - [Background](#background)
  - [Branching concept](#branching-concept)
    - [Branches](#branches)
  - [- shall integrate `8.0-beta-test-automation` and `gis-integration` upstream](#--shall-integrate-80-beta-test-automation-and-gis-integration-upstream)
    - [CD](#cd)
  - [Test-Automation Status](#test-automation-status)

## TL;DR

Adding features to [Laboratory for Simulation Development - LSD repo](https://github.com/marcov64/Lsd) with the aim to improve the rigor and reliability of this, in my opinion outstanding, agent-based modelling framework, at one end (CI-CD, but in a practical manner), and improve its usability for scientists and reviewers (features, model configuration management, automated analysis), on the other hand.

GIS, because the geographic representation capabilities are perhaps one of the most missed features in the core LSD and also perhaps the biggest feature that exists as MVP in the GIS-beta branch.

## Background

This is a fork of the [Laboratory for Simulation Development - LSD rep](https://github.com/marcov64/Lsd). Originaly I added some features to this agent-based modelling suite to foster my reasearch for my PhD and beyond. Now, I am working on porting those features back to the core LSD - in a slow pace, as my time is limited.

This file will serve as a kind of roadmap and summary for this endeavour and will grow as I find time and see fit...


## Branching concept

### Branches

- [GIS-master](https://github.com/FrederikSchaff/Lsd/tree/GIS-master): Contains the current state of the LSD initially forked and developed for my PhD and future work.
  - Latest **release**: [LSD-GIS-v0.21](https://github.com/FrederikSchaff/Lsd/releases/tag/LSD-GIS-v0.21)
  - Zenodo (for citations): [![DOI](https://zenodo.org/badge/148465802.svg)](https://zenodo.org/badge/latestdoi/148465802)
- [GIS-beta](https://github.com/FrederikSchaff/Lsd/tree/GIS-beta): Development branch for GIS-master
  - bug-fixes only (as planned)

----
- [8.0-beta](https://github.com/FrederikSchaff/Lsd/tree/8.0-beta): direct fork of [upstream 8.0-beta](https://github.com/marcov64/Lsd/tree/8.0-beta)
  - bug-fixes only for upstream merge.
- [8.0-beta-test-automation](https://github.com/FrederikSchaff/Lsd/tree/8.0-beta-test-automation): CI-CD branch
  - improved development experience
  - github actions (CI)
- [8.0-beta-integration](https://github.com/FrederikSchaff/Lsd/tree/8.0-beta-integration): Integrate changes for upstream merge.
  - shall integrate `8.0-beta-test-automation` and `gis-integration` upstream
----
- [gis-integration-project](https://github.com/FrederikSchaff/Lsd/tree/gis-integration-project): The branch containing, e.g., this file.
  - contain `8.0-beta-integration` plus some additional documentation
  - track status of integration
- [gis-integration-common-base](https://github.com/FrederikSchaff/Lsd/tree/gis-integration-common-base): Common reference for [8.0-beta] and [8.0-beta-integration].
  - Currently some 7.1 beta
  - Aim (perhaps) forward step-by-step as integration of GIS-beta commences, until we are equal with then current upstream LSD beta.
- [gis-integration](https://github.com/FrederikSchaff/Lsd/tree/gis-integration): Integration branch for portation of GIS-beta to upstream LSD
  - without project documentation

### CD

The idea is to not work on any of the branches directly (except `gis-integration-project`) but create task-based branches, following the pattern:

- `gis-integration/TASK`
- `test-automation/TASK`
- `hotfix/TASK`

and merging them back in the respective branches via pull requests. 

commits should have prefixes:
- `project`: Documentation on project state
- `test-automation`: adding tests / improving CI-CD
- `(bug)fix`: fixing something
- `improvement`: any thing adding new functionality.
Also, at best at the beginning, link to a feature/improvement with an identifier / unique name.

Depending on the size of the tasks, I may also decide to make use of the issue tracker and link them via issue numbers. Let's see.


## Test-Automation Status

_Note_: This may not be up to date! And not all is automated.

| Metric     | Coverage |
|------------|----------|
| LSD Macros | ? / ?    |
| GIS Macros | 0 / 0    |


