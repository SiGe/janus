# Experiment files 
The experiment files are in INI file format.  As to any INI file, there are a few sections with a set of attributes within each section.  The experiments/ folder has few examples of such experiment files.  OBSOLETE does not mean that the values should be omitted, but rather, they are not used.  Please make sure to specify even the OBSOLETE values.

## [general]
```traffic-test```: Traffic file used for the experiments.

```traffic-training```: OBSOLETE.  This option could be useful for building a traffic predictor model later on.  As is, it does not do anything.

```network```:  The configuration of the network used.  Right now the only supported option is jupiter.  And it follows the following format: ```jupiter-#core-#pod-#agg/pod-#tor/pod-bw```, so to create a jupiter topology with 4 cores, 6 pods, 8 aggs per pod, 10 tors per pod, and a link bandwidth of 10Gbps, we could use: ```jupiter-4-6-8-10-10000000000```.

```mop-duration```: The duration of the mop.  The default value for this is 1.  This can greatly impact the accuracy of the results based on how well the predictor works.

## [failure]
```concurrent-switch-failure```: Maximum number of concurrent failures to consider.  Janus should give you an error if this number is too low.

```concurrent-switch-probability```: Probability of concurrent independent switch failure.

```failure-mode```: Switch failure mode.  The only supported model right now is ```independent```.

```failure-warm-cost```: OBSOLETE.

## [scenario]
```time-begin```: Start time of the experiment.

```time-end```: End time for the experiment.

```time-step```: Distance between two consecutive experiments.

In total Janus will execute the change (```(time-end - time-begin) / time-step```) number of times.

## [predictor]
```type```: The only supported predictor type is ```perfect```, which gives the exact traffic matrix for the next interval.

```ewma-coeff```: OBSOLETE.

## [criteria]
```promised-throughput```: The amount of minimum promised throughput to a
ToR-to-ToR "flow".  If the flow requests lower than this number and receives
less than what it asks for, it is considered in violation.

```risk-violation```: This is the SLO violation risk function.  There are many
supported functions.  The most popular one being the stepped function.  The
format of the step function is: ```stepped[-pair]*``` where each pair specifies
the expected SLO together with the associated violation if that SLO is not
received.  For example, to specify an SLO function with zero penalty for 100 to 99.99, 10 penalty for 99.99 to 99.9, 20 penalty for 99.9 to 99.5, 30 penalty for 99.5 to 99, and 100 penalty otherwise, we could use the following specification. 

```stepped-0/100-99/30-99.5/20-99.9/10-99.99/0-100/0```

```criteria-time```: The only supported option is ```cutoff-at-[XX]```.  ```cutoff-at-[XX]``` means that we want to bound the plan to XX steps.  It is possible to specify the "cost" of being late by assigning a cost value to each step.  E.g., ```cutoff-at-8/1000,1000,1000,1000,1000,1000,1000,1000```, specifies that each step of the plan costs 1000 units.


```criteria-length```: The only valid option is ```maximize```.  This means that Janus prefers plans that have longer lengths.

```risk-delay```: OBSOLETE.

## [cache]
```rv-cache-dir```: Cache folder for random variable files that long-term generates.

```ewma-cache-dir```: OBSOLETE.

```perfect-cache-dir```: OBSOLETE.

## [upgrade]

```switch-group```: A repeatable option that specifies which set of switches we want to upgrade.  It uses the block format specified in the paper.  Format is: switchType-location-count-color.  switchType for ```jupiter``` network is either ```pod/agg``` or ```core```.  ```location``` is the aggregate pod number for that switch OR 0 for core switches.  Count is the number switches in that location (block) to upgrade.  ```Color``` is for building superblocks.  Blocks of switches that should be upgraded together.  There is no right way of setting the color.  Typically for Jupiter it's better to allow the planner to upgrade aggs together and cores separately.  It is possible to use finer grained plannings, e.g., color high traffic pods together or whatnot.

For example, to upgrade 8 core switches and 8 aggregate switches in 8 pods (pod 0 to 7), we can use the following configuration.  We use two colors here: 0 and 1.

```
switch-group = core-0-8-0
switch-group = pod/agg-0-8-1
switch-group = pod/agg-1-8-1
switch-group = pod/agg-2-8-1
switch-group = pod/agg-3-8-1
switch-group = pod/agg-4-8-1
switch-group = pod/agg-5-8-1
switch-group = pod/agg-6-8-1
switch-group = pod/agg-7-8-1
```

```freedom```: Freedom is the granularity of planning for each superblock.  So a freedom group of ```8-4``` means that we can use upto 8 steps to upgrade the first superblock (in the previous example core switches with color 0) and upto 4 steps to upgrade all the aggregate switches (specified with color 1).
