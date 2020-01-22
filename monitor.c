#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <wait.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <ctype.h>
#include "monitor.h"

void printUsage()
{
   fprintf(stderr,"Usage: monitor [-s] <executable> <interval> <logFilename>\n");
   exit(EXIT_FAILURE);
}

void errorMsg(char* name, char* type)
{
   perror(NULL);
   fprintf(stderr,"%s %s error\n",name,type);
   exit(EXIT_FAILURE);
}

int getInterval(int argc, char** argv)
{
   int interval = 0;
   
   interval = strtol(argv[argc-2],&argv[argc-2],10);

   if (argv[argc-2][0] != 0)
      printUsage();

   return interval;
}

char* getProcess(int argc, char** argv)
{
   char* process;

   if ((process = malloc(strlen(argv[argc-3] + 1))) == NULL)
      errorMsg(argv[argc-3],"malloc");

   return strcpy(process,argv[argc-3]);   
}

FILE* getOutputFile(MonitorTools *mt, int argc, char** argv)
{
   FILE* ret;

   if ((mt->outputFileName = malloc(strlen(argv[argc-1]) + 1)) == NULL)
      errorMsg("outputFileName","malloc");
   
   mt->outputFileName = strcpy(mt->outputFileName,argv[argc-1]);

   if ((mt->outputFileFd = open(mt->outputFileName, O_WRONLY | O_CREAT , 0640))
      == -1)
      errorMsg("outputFile","open");

   if ((ret = fdopen(mt->outputFileFd,"wr")) == NULL)
      errorMsg("outputFile","fdopen");

   return ret;
}

FILE *openMetricsFile(char *path, int *fd)
{
   FILE* ret;

   if ((*fd = open(path, O_RDONLY))
      == -1)
      errorMsg("outputFile","open");

   if ((ret = fdopen(*fd,"r")) == NULL)
      errorMsg("outputFile","fdopen");

   return ret;  
}
    
MonitorTools *setMonitorTools(int argc, char** argv)
{
   MonitorTools *mt;

   if (argc > 5 || argc < 4)
      printUsage();

   if ((mt = malloc(sizeof(MonitorTools))) == NULL)
      errorMsg("MonitorTools struct","malloc");

   mt->systemMetricsFlag = 0;
  
   if (argc == 5 && strcmp(argv[1],"-s") != 0)
      printUsage();

   if (strcmp(argv[1],"-s") == 0)
      mt->systemMetricsFlag = 1;

   mt->interval = getInterval(argc,argv);
   mt->process = getProcess(argc,argv);
   mt->outputFile = getOutputFile(mt,argc,argv);

   return mt;
}

void freeMt(MonitorTools *mt)
{
   free(mt->process);
   free(mt->outputFileName);
   free(mt);
}

pid_t runProcess(MonitorTools *mt)
{
   pid_t pid;

   if ((pid = fork()) == -1)
      errorMsg(mt->process,"fork");

   if (pid == 0)
      if (execl(mt->process,mt->process,(char*)NULL) == -1)
         errorMsg(mt->process,"exec"); 
      
   return pid; 
}

void bufDate(MonitorTools *mt, char* buf)
{
   int i = 0;
   time_t t;
   char temp[30];
   temp[0] = '\0';

   strcat(buf,"[");

   time(&t);
   strcat(temp,ctime(&t));
   temp[strlen(temp) - 1] = ']';

   while (isprint(temp[i++]) == 0)
      temp[i] = '\0';

   strcat(buf,temp);
   strcat(buf," ");
}

void clearWhitespace(FILE *fp, char *buf)
{
   int c = ' ';

   while (c == ' ')
      c = fgetc(fp);
}

void bufName(char *buf, char *metricName)
{
   strcat(buf,metricName);
   strcat(buf," ");
}

void bufNumber(FILE *fp, char *buf)
{
   // 48-57 inclusive is ascii 0-9 characters
   int c, i = 0; 
   char temp[30];

   while (((c = fgetc(fp)) >= 48 && c <= 57) || c == '.')
      temp[i++] = c; 

   ungetc(c,fp);

   if (i > 0)
   {
      temp[i] = ' ';
      temp[i+1] = '\0';
      strcat(buf,temp);
   }
}

void formatDecimal(char *strNum, int numOfDecimalPlacesToAdd)
{
   int i = 0, k = 0, pastDecimal = 0, nodpta = numOfDecimalPlacesToAdd;
  
   while (strNum[i++] != '\0')
   {
      if (pastDecimal)
         k++;

      if (strNum[i] == '.')
         pastDecimal = 1; 
   }
  
   for (i=0; i<=(nodpta-k); i++)
      strcat(strNum,"0");
} 

void buf_proc_diskstats_metrics(MonitorTools *mt, char *buf)
{
   FILE *fp;
   int c, fd, i = 0, k = 0, atSdaLine = 0, columnNum = 0;
   char *metricNames[6] = {"totalnoreads","totalsectorsread","nomsread",
      "totalnowrites","nosectorswritten","nomswritten"};
   char temp[30];

   // when local, use local "diskstatsFile" instead of path to /proc/diskstats 
   fp = openMetricsFile("diskstatsFile",&fd);

   // need metrics from /proc/diskstats file at row "sda," columns 1,3,4,5,7,8

   strcat(buf,"[DISKSTATS(sda)] ");

   while ((c = fgetc(fp)) != 's')
      ;

   temp[i++] = c;

   while (!atSdaLine)
   {
      c = fgetc(fp);
      temp[i++] = c;
      temp[i] = '\0';
      if (strcmp(temp,"sda") == 0)
         atSdaLine = 1; 
      if (c == ' ' || c == '\n')
      {
         temp[0] = '\0';
         i = 0;
      }
   }
   
   while (columnNum < 9)
   {
      if ((c = fgetc(fp)) == ' ')
      {
         columnNum++;
         while (c == ' ')
            c = fgetc(fp); 
         ungetc(c,fp);
         switch (columnNum)
         {
            case 1:
            case 3:
            case 4:
            case 5:
            case 7:
            case 8:
               bufName(buf,metricNames[k]);
               bufNumber(fp,buf);
               if (++k > 10)
                  k--;
         }
      }
   }
   close(fd);
}

void buf_proc_loadavg_metrics(MonitorTools *mt, char *buf)
{
   FILE *fp;
   int c, fd, i = 0, k = 0, columnNum = 1;
   char *metricNames[3] = {"1min","5min","15min"};
   char temp[30];
   
   fp = openMetricsFile("/proc/loadavg",&fd);

   // need metrics from /proc/loadavg file at columns 1,2,3

   strcat(buf,"[LOADAVG] ");

   while (columnNum < 4)
   {
      if ((c = fgetc(fp)) != ' ')
      {
         temp[i++] = c;
         temp[i] = '\0';
      }
      else
      {
         columnNum++;
         bufName(buf,metricNames[k]);
         if (++k > 2)
            k--;
   
         formatDecimal(temp,6);
         strcat(temp," ");
         strcat(buf,temp);
         temp[0] = '\0';
         i = 0;
      }
   }
   close(fd);
}

void buf_proc_meminfo_metrics(MonitorTools *mt, char *buf)
{
   FILE *fp;
   int c, fd, i = 0, j = 0, lineNum = 1;
   char *searchStrings[6] = {"MemTotal","MemFree","Cached","SwapCached",
      "Active","Inactive"};
   char *metricNames[6] = {"memtotal","memfree","cached","swapcached",
      "active","inactive"};
   char temp[30];

   fp = openMetricsFile("/proc/meminfo",&fd);

   // need metrics from /proc/meminfo file at lines 1,2,5,6,7,8

   strcat(buf,"[MEMORY] ");

   while (lineNum < 9)
   {
      
      if ((c = fgetc(fp)) == '\n')
         lineNum++;

      temp[i++] = c;
      temp[i] = '\0';

      if (strcmp(temp,searchStrings[j]) == 0)
      {
         bufName(buf,metricNames[j]);
         c = fgetc(fp);
         while ((c = fgetc(fp)) == ' ')
            ;
         ungetc(c,fp);
         bufNumber(fp,buf);
         if (++j > 5)
            j--;
      }

      if (c == ' ' || c == '\n')
      {
         temp[0] = '\0';
         i = 0;
      }
   }
   close(fd);
}
   
void buf_proc_stat_metrics(MonitorTools *mt, char *buf)
{
   FILE *fp;
   int c, fd, i = 0, j = 0, k = 0, columnNum = 1;
   char *searchStrings[5] = {"intr","ctxt","processes","procs_running",
      "procs_blocked"};
   char *metricNames[11] = {"cpuusermode","cpusystemmode","idletaskrunning",
      "iowaittime","irqservicetime","softirqservicetime","intr","ctxt",
      "forks","runnable","blocked"};
   char temp[30];

   fp = openMetricsFile("/proc/stat",&fd);

   // need metrics from /proc/stat file at lines 1,3,4,5,6,7, as well as 
   // from names intr,ctxt,processes,procs_running,procs_blocked

   while (columnNum < 9)
   {
      c = fgetc(fp);
      if (c == ' ')
      {
         columnNum++;
         while (c == ' ')
            c = fgetc(fp); 
         ungetc(c,fp);
         switch (columnNum - 1)
         {
            case 1:
            case 3:
            case 4:
            case 5:
            case 6:
            case 7:
               bufName(buf,metricNames[j]);
               bufNumber(fp,buf);
               if (++j > 10)
                  j--;
         }
      }
   }

   while ((c = fgetc(fp)) != EOF)
   {
      if (c == ' ' || c == '\n')
      {
         temp[0] = '\0';
         i = 0;
      } 
      else
      {
         temp[i++] = c;
         temp[i] = '\0';
         
         if (strcmp(searchStrings[k],temp) == 0)
         {
            c = fgetc(fp);

            bufName(buf,metricNames[j]);
            bufNumber(fp,buf);

            if (++j > 10)
               j--;
            if (++k > 4)
               k--;
         }
      }
   }
   close(fd);
} 

void outputSystemMetrics(MonitorTools *mt)
{
   char *buf = malloc(1E+3);
   buf[0] = '\0';

   bufDate(mt,buf);

   strcat(buf,"System [PROCESS] ");

   // add needed metrics from /proc/stat file to buffer
   buf_proc_stat_metrics(mt,buf);

   // add needed metrics from /proc/meminfo file to buffer 
   buf_proc_meminfo_metrics(mt,buf);

   // add needed metrics from /proc/loadavg file to buffer
   buf_proc_loadavg_metrics(mt,buf);

   // add needed metrics from /proc/diskstats file to buffer
   buf_proc_diskstats_metrics(mt,buf);

   strcat(buf,"\n\0");
   fputs(buf,mt->outputFile);
   free(buf);
}

void buf_proc_pid_stat_metrics(MonitorTools *mt, char *buf)
{
   FILE *fp;
   int c, fd, j = 0, columnNum = 1;
   char *metricNames[11] = {"executable","state","minorfaults","majorfaults",
      "usermodetime","kernelmodetime","priority","nice","numthreads",
      "vsize","rss"};
   char temp[30], path[20];

   sprintf(path, "/proc/%d/stat",(int)mt->pid);

   fp = openMetricsFile(path,&fd);

   // need metrics from /proc/[pid]/stat file (after "(bash) S" for example))
   // at columns 2,3,10,12,14,15,18,19,20,23,24

   strcat(buf,"[STAT] ");

   while ((c = fgetc(fp)) != ')')
      ;

   while ((c = fgetc(fp)) == ' ')
      ;

   // column 2, executable 
   strcat(buf,metricNames[j++]);
   sprintf(temp, " (%s) ",mt->process);
   strcat(buf,temp);

   // column 3, state
   strcat(buf,metricNames[j++]);
   sprintf(temp, " %c ",c);
   strcat(buf,temp);

   columnNum = 3;

   while (columnNum < 25)
   {
      c = fgetc(fp);
      if (c == ' ')
      {
         columnNum++;
         while (c == ' ')
            c = fgetc(fp); 
         ungetc(c,fp);
         switch (columnNum)
         {
            case 10:
            case 12:
            case 14:
            case 15:
            case 18:
            case 19:
            case 20:
            case 23:
            case 24:
               bufName(buf,metricNames[j]);
               bufNumber(fp,buf);
               if (++j > 10)
                  j--;
         }
      }
   }

   close(fd);
}

void buf_proc_pid_statm_metrics(MonitorTools *mt, char *buf)
{
   FILE *fp;
   int c, fd, j = 0, columnNum = 1;
   char *metricNames[5] = {"program","residentset","share","text","data"};
   char path[20];

   sprintf(path, "/proc/%d/statm",(int)mt->pid);

   fp = openMetricsFile(path,&fd);

   // need metrics from /proc/[pid]/statm file at columns 1,2,3,4,6 

   strcat(buf,"[STATM] ");

   while (columnNum < 7)
   {
      if (columnNum != 5 && columnNum != 7)
      {
         bufName(buf,metricNames[j]);
         if (++j > 4)
            j--;
         bufNumber(fp,buf);
      }
      else
         while (fgetc(fp) == ' ')
            ;

      if ((c = fgetc(fp)) == ' ')
         columnNum++;
      else
         ungetc(c,fp);
   }

   close(fd);
}
  
void outputProcessMetrics(MonitorTools *mt)
{
   char *buf = malloc(1E+3);
   char processString[20];
   buf[0] = '\0';
   
   bufDate(mt,buf);

   sprintf(processString, "Process(%d) ",(int)mt->pid);
   strcat(buf,processString);

   // add needed metrics from /proc/[pid]/stat file to buffer
   buf_proc_pid_stat_metrics(mt,buf);

   // add needed metrics from /proc/[pid]/statm file to buffer
   buf_proc_pid_statm_metrics(mt,buf);

   strcat(buf,"\n\0");
   fputs(buf,mt->outputFile);
   free(buf);
}

int main(int argc, char** argv)
{
   // retrieves command line args and retains/organizes the info for later use
   MonitorTools *mt = setMonitorTools(argc,argv);
   int status;

   mt->pid = runProcess(mt);

   while (waitpid(mt->pid,&status,WNOHANG) != mt->pid)
   {
      if (mt->systemMetricsFlag)
         outputSystemMetrics(mt);
      else
         outputProcessMetrics(mt);
      
      usleep(mt->interval);
   }

   freeMt(mt);

   return 0;
} 
