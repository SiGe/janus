# What is an experiment?

An experiment is an instance of a change plan executed repeatedly for a set
number of times.  Typically, operators only execute a change once, but for the
purposes of benchmarking, we have options for running a change repeatedly over
different hours of the day.

# Experiment files 
We use INI files to define experiments.  As to any INI file, there are sections
with a set of attributes per each section.  The `experiments/` folder comes with
a few examples.  Below, we will discuss the attributes within each section.
Some of the attributes are obsolete and marked as such (OBSOLETE), however, they
still need to be specified to ensure that Janus runs smoothly.

## [general]
`traffic-test`: Traffic file used for the experiments.  The format of the
traffic files and how to generate new traffic files is explained in
[TRAFFIC](docs/TRAFFIC.md).

`traffic-training`: OBSOLETE.  This option maybe used in the future for
training the traffic predictor.  As is, this option is useless as there is no
real-time traffic prediciton in the system.

`mop-duration`: The duration of the subplan in the number of traffic matrices.
For example, with a value of one, we only need to simulate the impact of a
subplan for a single traffic matrix. The default value for this is 1.  This can
greatly impact the accuracy of the results based on how well the predictor
works.

Janus's notion of passage of time is strictly defined through passage of traffic
matrices.  Janus does not need to know the number of seconds between two
consecutive traffic matrices---it is the job of the operator to ensure that all
the values are scaled accordingly.  For example, in the paper we aggregated the
traffic matrices once every 5 minutes.  This has several consequences:

1) The length of a subplan (an operation) is expressed through the number of
traffic matrices that we have to simulate for that subplan.
2) For finer grained SLO monitoring (e.g., 1 minute intervals), operators may
want to increase the number of intermediate traffic matrices (e.g., 5 TMs
instead of 1 TM).  This naturally makes traffic prediction more complicated---a
less confident traffic predictor results in a slower Janus.
4) If routing changes happen often enough (and are impactful enough that cause
large scale traffic shifts), it would make sense to reduce the length of a
traffic matrix interval to increase the fidelitly of simulations.

`network`:  The network topology and configuration used.  The only supported
option as of the moment is `jupiter`.  It follows the following format:
`jupiter-#core-#pod-#agg/pod-#tor/pod-bw`.  For example, to create a jupiter
topology with 4 cores, 6 pods, 8 aggs per pod, 10 tors per pod, and a link
capacity of 10Gbpi, we would use: `jupiter-4-6-8-10-10000000000`.

Gpbi is the number of bits that a link can pass per traffic matrix interval, as
discussed in the previous attribute (mop-duration).


## [failure]
`concurrent-switch-failure`: Maximum number of concurrent switch failures to
consider.  Janus will throw an error if this number is too low (i.e., the
total number of switches to consider result in less than [90% of scenarios](https://github.com/SiGe/janus/blob/master/src/failure.c).

`concurrent-switch-probability`: Probability of concurrent independent switch failure.

`failure-mode`: Switch failure mode.  The only supported model right now is
`independent`.  There is also code for `warm` reboots, but that is not tested
and is probably the wrong model.

`failure-warm-cost`: OBSOLETE.  Check `failure-mode`.

## [scenario]
`time-begin`: Start time of the experiment.

`time-end`: End time for the experiment.

`time-step`: Distance between two consecutive experiments.

In total Janus will execute the change (`(time-end - time-begin) / time-step`) number of times.

## [predictor]
`type`: The only supported predictor type is `perfect`, which gives the exact
traffic matrix for the next interval.

`ewma-coeff`: OBSOLETE.

## [criteria]
`promised-throughput`: The amount of minimum promised throughput to a
ToR-to-ToR "flow".  If the flow requests lower than this number and receives
less than what it asks for, it is considered in violation.

`risk-violation`: This is the SLO violation risk function.  For a list of
supported functions have a look at
[risk.c](https://github.com/SiGe/janus/blob/master/src/risk.c#L215-L227).  Maybe
the most popular function is the stepped function.  The format of the step
function is: `stepped[-pair]*` where each pair specifies the expected SLO
together with the associated violation if that SLO is not received.  For
example, to specify an SLO function with zero penalty for 100 to 99.99, 10
penalty for 99.99 to 99.9, 20 penalty for 99.9 to 99.5, 30 penalty for 99.5 to
99, and 100 penalty otherwise, we could use the following specification:

`stepped-0/100-99/30-99.5/20-99.9/10-99.99/0-100/0`

**Note**: SLOs in Janus are throughput SLOs and defined based on max-min
fairness simulations of traffic matrices.

`criteria-time`: The only supported option is `cutoff-at-[XX]`.
`cutoff-at-[XX]` means that we want to bound the plan to XX steps.  It is
possible to specify the "cost" of being late by assigning a cost value to each
step.  E.g., `cutoff-at-8/1000,1000,1000,1000,1000,1000,1000,1000`, specifies
that the plan should finish in at most 8 steps and each step of the plan costs
1000 units.


`criteria-length`: The only valid option is `maximize`.  This means that Janus
prefers plans that have longer lengths (but are within the deadline, i.e.,
`cutoff-at-[XX]`).

`risk-delay`: OBSOLETE.

## [cache]
`rv-cache-dir`: Cache folder for random variable files that long-term generates.
More details on this on [ARCH.md](docs/ARCH.md).

`ewma-cache-dir`: OBSOLETE.

`perfect-cache-dir`: OBSOLETE.

## [upgrade]

`switch-group`: A repeatable option that specifies which set of switches we want
to upgrade.  Format is: switchType-location-count-color.  switchType for
`jupiter` network is either `pod/agg` or `core`.  `location` is the aggregate
pod number for that switch OR 0 for core switches.  Count is the number switches
in that location (block) to upgrade.  `Color` is for building superblocks.
sets of blocks that should be upgraded together---there is no right way of
setting the color.  Typically for Jupiter it's better to allow the planner to
upgrade aggs together and cores separately.  It is possible to use finer grained
plannings, e.g., color high traffic pods together or whatnot.

For example, to upgrade 8 core switches and 8 aggregate switches in 8 pods (pod
0 to 7), we can use the following configuration.  We use two colors here: 0 and
1.

`
switch-group = core-0-8-0
switch-group = pod/agg-0-8-1
switch-group = pod/agg-1-8-1
switch-group = pod/agg-2-8-1
switch-group = pod/agg-3-8-1
switch-group = pod/agg-4-8-1
switch-group = pod/agg-5-8-1
switch-group = pod/agg-6-8-1
switch-group = pod/agg-7-8-1
`

`freedom`: Freedom is the granularity of planning for each superblock.  So a
freedom group of `8-4` means that we can use upto 8 steps to upgrade the first
superblock (in the previous example core switches with color 0) and upto 4 steps
to upgrade all the aggregate switches (specified with color 1).
