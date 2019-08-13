# Janus: A Risk Based Planner for Data Centers

Janus is a planner and a network risk emulator for data centers.  As is, Janus
exclusively supports Jupiter topology from Google's Jupiter rising paper.

## Getting Started

These instructions will get you a copy of the project up and running on your local machine for development and testing purposes. See deployment for notes on how to deploy the project on a live system.

### Prerequisites

The repository is self-contained.  It should successfully compile with gcc and clang on Linux (tested with Ubuntu 18.04, gcc 7.4, and Clang 6.0).

### Compilation

Compilation is a matter of invoking make:
```
make -j\[number of threads\]
```

This creates a binary file (netre) in the bin/ folder.

## Usage

```
> ./bin/netre <experiment-setting ini file> [OPTIONS]
```

Only two options are available: -a \[planner\] and -x

-a select the planner to use.  The available options are: long-term, pug,
pug-lookback, pug-long, ltg.

- long-term generates cache-files.
- ltg is the MRC planner discussed in the paper.
- pug-lookback is Janus.  Our traffic aware planner.
- pug-long is Janus offline.  A traffic aware planner that only uses historical
  traffic data for planning.
- pug is Janus but instead of using recent historical traffic data it uses all
  the available historical traffic data.  It can be useful when traffic
  variations are high.  This is not discussed in the paper.

## Experiments

To run experiments, after running make, go to the script folder and execute the
relevant experiments.

## Authors

* **Omid Alipourfard** - *Initial work* - [SiGe](https://omid.io)

## License

This project is licensed under the MIT License - see the [LICENSE.md](LICENSE.md) file for details
