This program is used to keep track of the how much memory certain processes
consume, along with its relative consumption compared to the system. It will 
record either systemwide or process specific performance
statistics based on the parameters passed (see usage). 

The systemwide performance statistics are found in /proc/* files, whereas 
process specific statistics are found in /proc/"pid"/stat and /proc/"pid"/stam
files, with the "pid" replaced with the process id of the particular
process to examine. 

All recorded statistics will be output to a file
specified in the last parameter of this program (see usage).
