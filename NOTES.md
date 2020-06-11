## Type of blocks

### TIMED (DEFAULT) 

- Programs that spit something to `stdout` once, then exit
- Their output is expected to potentially change between runs 
- Identified by a positive `reload` value in their config
- Example: block that returns the current CPU temperature

We run these over and over again in a set time interval, as set via `reload` in  
their configuration. After they have produced output for the first time, they 
should exit, but we should make sure they really did end, for example by using 
`kill()`, so as to not produce a ton of idling processes. Once we are sure they 
are dead, we should mark them as such. The (last line of) output they produced 
shall be buffered, so we can compare the output of two runs. If the output did 
not change, we don't need to update bar (unless another block has new output).

### STATIC

- Programs that spit something to `stdout` once, then exit
- Their ouput is expected to be the same for the runtime of `succade`
- Identified by `reload = 0` in their config
- Example: block that returns the name and version of the Distro

For those, we want to grab the (laste line of) output, then make sure they did 
actually exit (for example, by sending a `kill()`) and marking them as `dead`.
We never run them again. Instead, we just re-use the same output over and over 
again, whenever it is time to update the bar. After fetching the output for the 
first and last time, we should make sure they really did exit, maybe via `kill`

In other words, these are basically the same as the DEFAULT blocks, with the 
only difference being that we run them only a single time.

### SPARKED (TRIGGERED)

- Programs that spit something to `stdout` once, then exit
- Their output is expected to potentially change between runs 
- Optionally, they can take (or require) input as command line argument/s
- Identified by having `trigger` set to some command/program in their config 
- Example: block that pretty-prints the volume returned by ALSA monitor

These blocks work similar to TIMED blocks, with two notable differences. First, 
instead of being run at a set interval, we only run them after another command 
(their `trigger` or spark) has produced new output. Second, when we run them, 
we don't just run their command as specified in the config, but we append the 
(last line of) their trigger command's output as a command line argument, in 
case the block wants to process that output in some way. 

Should a SPARKED block's trigger die, we treat the block like a dead STATIC 
block from that point on, with the last output it produced being used forever.

### LIVE

- Programs that keep running and printing new output as lines to `stdout`
- Their output is expected to potentially change between runs
- Identified by having `live = true` in their config file
- Example: block that keep printing the time and date every second/minute/...

For LIVE blocks, the `reload` and `trigger` configurtion options are ignored. 
These blocks will be run once and are expected to keep running until we tell 
them to exit. During their lifetime, they are expected to print new output as 
newline-delimited string (_lines_) to `stdout`. We therefore have to register 
them with epoll, so we are informed about new ouput being available - and only 
then do we process them. Again, they should not be killed after they have been 
processed. However, special attention has to be paid at the end of succade's  
lifetime, as all LIVE blocks should be killed then.

When a live block dies prematurely, we should mark it as dead and treat it as 
a STATIC block from that point onwards, re-using the last output it produced.

