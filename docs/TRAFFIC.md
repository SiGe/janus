# Traffic trace files

Janus has its own traffic trace file format.  The format is designed to make
working with large trace files easy.  There are two files: an index file and a
data file.  The index file uses .index suffix and data file uses .data suffix.

The separation of index and data files makes it easy to add or remove TMs as we
can always add the TMs to the end of the data file and rewrite a much smaller
index file to specify the location of our TM.

## Index file
The index file contains the byte position of different traffic matrices (TMs) in
the data file.  This allows Janus to move back and forth in the trace file with
ease.

Each index contains 3 information:

```c
// Index data structure for the trace
struct traffic_matrix_trace_index_t {
  // Location of the TM in the .data file
  uint64_t                seek;
  // Time associated with this TM
  trace_time_t            time;
  // Size of the TM, i.e., how much should we read
  uint64_t                size;
};
```

## Data file
The data file contains consecutive TMs.  The only thing separating the
boundaries of different TMs is the size value in the index file (so we cannot
recover the TMs from the data file alone). 

The TMs right now are in dense matrix format---but this might change in the
future.

### Traffic matrix

A traffic matrix is a list of consecutive float values (bw_t) in the current
file format.


# Generating traffic matrices
The repository contains a binary (traffic_compressor accessible through `make
traffic_compressor`) for exporting an easier to understand trace format (more on
this later) to Janus's format.

The format is as follows:

Each trace is a directory.  There are two types of files in that directory a
key.tsv where it specifies the what source talks with what destination.  And the
traffic matrix files where each line is a float value in text format.  The lines
in the traffic matrix and the key.tsv match.  So for example, a data center with
4 hosts and 2 TMs can have the following files:

```
trace_folder/
  key.tsv
  0001.tsv
  0002.tsv
  0003.tsv
  0004.tsv
``` 

There is a single file called key.tsv where Nth line number specifies traffic
between source tor/dest tor.

So in our previous example, we would have 12 lines in key.tsv (4 source hosts *
3	destination	hosts). And each line has 6 columns: `src_tor	dst_tor	src_pod
dst_pod	0	0`.  For example:

```
t1	t2	p1	p1	0	0
t1	t3	p1	p2	0	0
t1	t4	p1	p2	0	0
t2	t1	p1	p1	0	0
t2	t3	p1	p2	0	0
t2	t4	p1	p2	0	0
t3	t1	p2	p1	0	0
t3	t2	p2	p1	0	0
t3	t4	p2	p2	0	0
t4	t1	p2	p1	0	0
t4	t2	p2	p1	0	0
t4	t3	p2	p2	0	0
```

And each trace file would have 12 lines.  E.g., `0001.tsv` would be:
`
10.0
1.0
0.0
0.0
0.0
0.10
1.0
1.0
10.0
0.0
0.0
10.0
`

## Running traffic compressor
To run the traffic compressor on the above folder (e.g., trace/sample_trace), we could issue:
```bash
% mkdir trace/janus_trace

# It is important NOT to forget the trailing slash, i.e.:
# trace/sample_trace  DOES NOT work
# trace/sample_trace/ DOES work
% ./bin/traffic_compressor trace/sample_trace/ trace/janus_trace/traffic
```

This would generate two files `traffic.index` and `traffic.data` in the
`janus_trace` folder.  Ready for use with netre.
