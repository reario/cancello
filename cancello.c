#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <netdb.h>
#include <errno.h>
#include <signal.h>
#include <sysexits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <modbus.h>
#include "gh.h"

modbus_t *mb;
modbus_t *mb_otb;

int ts(char * tst, char * fmt)
{
  time_t current_time;
  char MYTIME[50];
  struct tm *tmp ;
  /* Obtain current time. */
  current_time = time(NULL);
  if (current_time == ((time_t)-1))
    {
      //logvalue(LOG_FILE, "Failure to obtain the current time\n");
      return -1;
    }
  tmp = localtime(&current_time);
  // [04/15/19 11:40AM] "[%F %T]"
  if (strftime(MYTIME, sizeof(MYTIME), fmt, tmp) == 0)
    {
      //logvalue(LOG_FILE,"Failure to convert the current time\n");
      return -1;
    }
  strcpy(tst,MYTIME);
  return 0;
}

void logvalue(char *filename, char *message)
{
  /*scrive su filename il messaggio message*/
  FILE *logfile;
  char t[30];
  ts(t,"[%F %T]");
  //ts(t,"[%Y%m%d-%H%M%S]");
  logfile=fopen(filename,"a");
  if(!logfile) return;
  fprintf(logfile,"%s %s",t,message);
  fclose(logfile);
}

int pulsante(modbus_t *m,int bobina) {

  char errmsg[100];

  if ( modbus_write_bit(m,bobina,TRUE) != 1 ) {
    // The modbus_write_bit() function shall return 1 if successful. 
    // Otherwise it shall return -1 and set errno.
    sprintf(errmsg,"ERRORE DI SCRITTURA:PULSANTE ON %s\n",modbus_strerror(errno));
    logvalue(LOG_FILE,errmsg);
    return -1;
  }

  sleep(1);

  if ( modbus_write_bit(m,bobina,FALSE) != 1 ) {
    sprintf(errmsg,"ERRORE DI SCRITTURA:PULSANTE OFF %s\n",modbus_strerror(errno));
    logvalue(LOG_FILE,errmsg);
    return -1;
  }

  return 0;
}

void rotate() {

  char t[20];
  char newname[40];
  char logmsg[100];
  int fd;

  ts(t,"%Y%m%d-%H%M%S");
  sprintf(newname,"cancello-%s.log",t);
  sprintf(logmsg,"Log rotation....archived to %s\n",newname);
  logvalue(LOG_FILE,logmsg);
  rename(LOG_FILE,newname);
  fd=open(LOG_FILE, O_RDONLY | O_WRONLY | O_CREAT,0644);
  close(fd);
}

void signal_handler(int sig)
{
  switch(sig) {

  case SIGHUP:
    // ----------------------------------
    rotate();
    logvalue(LOG_FILE,"new log file after rotation\n");
    break;
    // ---------------------------------
    
  case SIGTERM:
    logvalue(LOG_FILE,"terminate signal catched\n");
    unlink(LOCK_FILE);
    logvalue(LOG_FILE,"\tChiudo la connessione con PLC e OTB\n");
    modbus_close(mb);
    modbus_free(mb);
    logvalue(LOG_FILE,"\tLibero la memoria dalle strutture create\n");
    modbus_close(mb_otb);
    modbus_free(mb_otb);
    logvalue(LOG_FILE,"Fine.\n");
    logvalue(LOG_FILE,"--------------------------------------------------------------\n");      
    exit(EX_OK);
    break;

  }
}

void daemonize()
{
  int i,lfp;
  char str[10];
  if(getppid()==1) return; /* already a daemon */
  i=fork();
  if (i<0) exit(EX_OSERR); /* fork error */
  if (i>0) exit(EX_OK); /* parent exits */
  /* child (daemon) continues */
  setsid(); /* obtain a new process group */
  for (i=getdtablesize();i>=0;--i) close(i); /* close all descriptors */
  i=open("/dev/null",O_RDWR); dup(i); dup(i); /* handle standart I/O */
  umask(027); /* set newly created file permissions */
  // chdir(RUNNING_DIR); /* change running directory */
  lfp=open(LOCK_FILE,O_RDWR|O_CREAT,0640);
  if (lfp<0) exit(EX_OSERR); /* can not open */

  if (lockf(lfp,F_TLOCK,0)<0) {
    logvalue(LOG_FILE,"Cannot Start. There is another process running. exit\n");
    exit(EX_OK); /* can not lock */
  }

  /* first instance continues */
  snprintf(str,(size_t)10,"%d\n",getpid());
  write(lfp,str,strlen(str)); /* record pid to lockfile */
  
  signal(SIGCHLD,SIG_IGN); /* ignore child */
  signal(SIGTSTP,SIG_IGN); /* ignore tty signals */
  signal(SIGTTOU,SIG_IGN);
  signal(SIGTTIN,SIG_IGN);
  signal(SIGPIPE,SIG_IGN);
  signal(SIGHUP,signal_handler); /* catch hangup signal */
  signal(SIGTERM,signal_handler); /* catch kill signal */
}

int main (int argc, char ** argv) {

  uint16_t otb_in[10];
  char errmsg[100];
  uint16_t numerr = 0;

  // unit16_t plc_in[10];
  // system("echo \"PRIMNA di daemon\" | /usr/bin/mutt -s \"PRIMA di daemon\" vittorio.giannini@windtre.it");
 
  daemonize();

  mb = modbus_new_tcp("192.168.1.157",PORT);
  mb_otb = modbus_new_tcp("192.168.1.11",PORT);

  if ( (modbus_connect(mb) == -1) || (modbus_connect(mb_otb) == -1))
    {
      sprintf(errmsg,"ERRORE non riesco a connettermi con il PLC o OTB %s\n",modbus_strerror(errno));
      logvalue(LOG_FILE,errmsg);
      exit(EXIT_FAILURE);
    }

  while (1) {
    if ( modbus_read_registers(mb_otb, 0, 1, otb_in) < 0 ) {    // leggo lo stato degli ingressi collegati al wireless button
      numerr++;
      
      sprintf(errmsg,"\tERRORE Lettura Registro OTB per Cancello [%s]. Num err [%i]\n",modbus_strerror(errno),numerr);
      sleep(1);
      logvalue(LOG_FILE,errmsg);      

      if (numerr > 20) {
	logvalue(LOG_FILE,"-----------------------------------------------------------------------\n");      
	system("echo \"Errore di lettura nel registro OTB. Programma chiuso\" | /usr/bin/mutt -s \"Errore nella lettura del registro OTB\" vittorio.giannini@windtre.it");
	exit(EXIT_FAILURE);
      }
    } else {
      //-------------------------------------------------------------------------------------
      if (read_single_state((uint16_t)otb_in[0],OTB_IN8)) {
	logvalue(LOG_FILE,"APERTURA PARZIALE CANCELLO INGRESSO\n");
	if (pulsante(mb,APERTURA_PARZIALE)<0) {
	  logvalue(LOG_FILE,"\tproblemi con pulsante\n");
	}

      }
      //-------------------------------------------------------------------------------------      
      if (read_single_state((uint16_t)otb_in[0],OTB_IN9)) {
	logvalue(LOG_FILE,"APERTURA TOTALE CANCELLO INGRESSO\n");
	if (pulsante(mb,APERTURA_TOTALE)<0) {
	  logvalue(LOG_FILE,"\tproblemi con pulsante\n");
	}
      }
      //-------------------------------------------------------------------------------------  
      numerr=0;
    } // else
  } // while

  return 0;

}


