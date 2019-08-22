# Experiment files 

## [general]
```traffic-test```
```traffic-training```
```network```
```mop-duration```

## [failure]
```concurrent-switch-failure```: Maximum number of concurrent failures to
consider.  Janus should give you an error if this number is too low.

```concurrent-switch-probability```: Probability of concurrent independent
switch failure.

```failure-mode```: Switch failure mode.  The only supported model right now is ```independent```.

```failure-warm-cost```: OBSOLETE.

## [scenario]
```time-begin```: Start time of the experiment.
```time-end```: End time for the experiment.
```time-step```: Distance between two consecutive experiments.

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
received.  For example, to specify an SLO function that 

stepped-0/100-99/30-99.5/20-99.9/10-99.99/0-100/0
;
; Azure's risk function
; risk-violation=stepped-0/30-99/10-99.99/0-100/0

; Linear risk function
; risk-violation=linear-10000
risk-violation=stepped-0/100-95/25-99/10-99.95/0-100/0

; Plan time criteria
; Supported types are:
;   cutoff-at-[XX] which means that we want to bound the plan to XX steps
criteria-time=cutoff-at-8
; criteria-time=cutoff-at-8/1000,1000,1000,1000,1000,1000,1000,1000

; Plan length criteria
criteria-length=maximize

; Risk delay
risk-delay=dip-at-20

[cache]
rv-cache-dir = trace/data/QQ-static-compressed/cache-ltg/
ewma-cache-dir = trace/data/QQ-static-compressed/ewma-ltg/
perfect-cache-dir = trace/data/QQ-static-compressed/perfect-ltg/

[upgrade]
; Switch upgrade list.  Format is: swtype-location-count-color
; swtype can be core or pod/agg at the moment
switch-group = core-0-8-0
switch-group = pod/agg-0-8-1
switch-group = pod/agg-1-8-1
switch-group = pod/agg-2-8-1
switch-group = pod/agg-3-8-1
switch-group = pod/agg-4-8-1
switch-group = pod/agg-5-8-1
switch-group = pod/agg-6-8-1
switch-group = pod/agg-7-8-1

; Freedom is the granularity of planning for jupiter topology
freedom = 8-8
