# Janus: A Risk Based Planner for Data Centers

Janus is a repository of planners together with a network risk emulator for
data centers.  As is, Janus exclusively supports Jupiter topology from Google's
Jupiter rising paper.

## Getting Started

These instructions will get you a copy of the project up and running on your local machine for development and testing purposes. See deployment for notes on how to deploy the project on a live system.

### Prerequisites

The repository is self-contained.  It should successfully compile with gcc and clang on Linux (tested with Ubuntu 18.04, gcc 7.4, and Clang 6.0).

To download the traffic data, please run the setup script:

```
% ./setup.sh
```

### Compilation

Compilation is a matter of invoking make:
```
% make
```

This creates a binary file (netre) in the bin/ folder.

By default, we use clang to compile.  To use a different compiler you can issue:
```
% export CC=gcc && make
```

## Usage

Typically, you would run the experiments in the scripts/ folder only.  However,
you can also use netre in standalone mode and with different traffic and
configuration files.  For standalone use, you can use netre binary as below.

```
> ./bin/netre <experiment-setting ini file> [OPTIONS]
```

The available options are: -a \[execution mode\] and -x

-a selects the execution mode for Janus.  Valid options are one of: long-term,
pug, pug-lookback, pug-long, ltg, stats.

- long-term generates cache-files that pug-\* variation of planners use.  It
  should always be the first command to invoke for a new traffic/config file.

- ltg is the MRC (Maximum Residual Capacity) planner discussed in the paper.
  This is a capacity aware planner but it only works for symmetrical plans.
  You shoud also note that when using this planner, the number of steps should
  divide the number of switches that you are upgrading in each pod.
- pug-lookback is the implementation of Janus discussed in the paper.
- pug-long is Janus offline.  It only uses historical traffic information for
  planning.
- pug is a variation of Janu planner that instead of using recent historical
  traffic data for planning, it uses all the available historical traffic data.
  It can be useful when traffic variations are high.  This is not discussed in
  the paper.
- stats returns statistics about the traffic file specified in the config file.

Finally, -x option returns a short explanation of the execution mode

## Experiments

To run the experiments, first run make, then go to the script folder and
execute the relevant experiments.  Here are the mappings between the
experiments and the figures in the paper:

- Figure 7-a: 15-static-many.sh
- Figure 7-b: 02-dynamic-experiment.sh
- Figure 7-c: 09-failure-sweep.sh
- Figure 8-a: 07-cloud-cost.sh
- Figure 8-b: 10-step-count.sh 
- Figure 8-c: 11-scale.sh
- Figure 8-d: 13-bursty.sh OR 14-bursty-more.sh
- Table    4: 12-scale-time.sh
- Figure 9  : Rollback experiment, requires modification to code.
- Figure 10 : 04-cost-time.sh

The scripts generate files in the scripts/data/ folder (most likely).

To generate the plots, make sure you have run all the experiments (except rollback).  Go to the scripts/paper-plots folder and run:

```
% ./gen-data.sh
```

After, you can use gnuplot on plt files in the same folder to create the plots.

```
% gnuplot *plt
```

## Running Custom Experiments

Each experiment uses an experiment file.  The format of the experiment file is
explained in [CONFIG](CONFIG.md).

## Authors

* **Omid Alipourfard** - *Initial work* - [SiGe](https://omid.io)

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details
