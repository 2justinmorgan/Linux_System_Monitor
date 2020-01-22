#ifndef PROG_2
#define PROG_2

typedef struct
{
   int interval;
   int systemMetricsFlag; // boolean if -s exists in argv
   char* process;  // name of the forked process to be examined
   pid_t pid;  // pid of forked process to be examined
   FILE* outputFile;  // file that receives monitoring statistics
   int outputFileFd;
   char* outputFileName;
} MonitorTools;

#endif
