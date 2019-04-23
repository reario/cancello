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
#include <sys/time.h>
#include <time.h>
#include <modbus.h>
#include "gh.h"

modbus_t *mb;
modbus_t *mb_otb_pc;

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

void myCleanExit(char * from) {
  logvalue(LOG_FILE,from);
  unlink(LOCK_FILE);
  logvalue(LOG_FILE,"\tChiudo la connessione con PLC e OTB su PC\n");
  modbus_close(mb);
  modbus_free(mb);
  logvalue(LOG_FILE,"\tLibero la memoria dalle strutture create\n");
  modbus_close(mb_otb_pc);
  modbus_free(mb_otb_pc);
  logvalue(LOG_FILE,"Fine.\n");
  logvalue(LOG_FILE,"****************** END READ FROM PC **********************\n");
  
}

void signal_handler(int sig)
{
  switch(sig) {

  case SIGHUP:
    // ----------------------------------
    rotate();
    logvalue(LOG_FILE,"new log file after rotation read from pc\n");
    break;
    // ---------------------------------
    
  case SIGTERM:
    myCleanExit("Terminate signal catched\n");
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
    logvalue(LOG_FILE,"Cannot Start. There is another process running. read from pc exit\n");
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
  logvalue(LOG_FILE,"****************** START READ FROM PC **********************\n");
}

int main (int argc, char ** argv) {

  uint16_t otb_pc_in[10];
  char errmsg[100];
  uint16_t numerr = 0;
  struct timeval response_timeout;

  // unit16_t plc_in[10];
  // system("echo \"PRIMNA di daemon\" | /usr/bin/mutt -s \"PRIMA di daemon\" vittorio.giannini@windtre.it");
 
  daemonize();

  mb = modbus_new_tcp("192.168.1.157",PORT);
  mb_otb_pc = modbus_new_tcp("192.168.1.103",PORT);

  /* Define a new timeout! */
  response_timeout.tv_sec = 10;
  response_timeout.tv_usec = 0;

  modbus_set_response_timeout(mb,     &response_timeout); // 5 seconds 0 usec
  modbus_set_response_timeout(mb_otb_pc, &response_timeout); // 5 seconds 0 usec

  if ( (modbus_connect(mb) == -1 ))
    {
      sprintf(errmsg,"ERRORE non riesco a connettermi con il PLC %s\n",modbus_strerror(errno));
      //logvalue(LOG_FILE,errmsg);
      myCleanExit(errmsg);
      exit(EXIT_FAILURE);
    }

  if ( (modbus_connect(mb_otb_pc) == -1))
    {
      sprintf(errmsg,"ERRORE non riesco a connettermi con l'OTB PC. Premature exit (il processo event è attivo?) [%s]\n",modbus_strerror(errno));
      //logvalue(LOG_FILE,errmsg);
      myCleanExit(errmsg);
      exit(EXIT_FAILURE);
    }

  while (1) {
    if ( modbus_read_registers(mb_otb_pc, 74, 1, otb_pc_in) < 0 ) {    // leggo lo stato degli ingressi collegati al wireless button

      numerr++;
      sprintf(errmsg,"ERRORE Lettura Registro OTB PC per Cancello [%s]. Num err [%i]\n",modbus_strerror(errno),numerr);
      logvalue(LOG_FILE,errmsg);

      modbus_close(mb_otb_pc);      
      modbus_free(mb_otb_pc);
      mb_otb_pc = modbus_new_tcp("192.168.1.103",PORT);
      response_timeout.tv_sec = 10;
      response_timeout.tv_usec = 0;
      modbus_set_response_timeout(mb_otb_pc, &response_timeout); // 10 seconds 0 usec

      /* while (modbus_connect(mb_otb)==0 || numerr<=15) { */
      /* 	sprintf(errmsg,"\tERRORE riconnessione OTB [%s]. Num err [%i]\n",modbus_strerror(errno),numerr); */
      /* 	logvalue(LOG_FILE,errmsg); */
      /* 	sleep(1); */
      /* 	numerr++; */
      /* } */
    
      if (modbus_connect(mb_otb_pc)==-1) {
	sprintf(errmsg,"\tERRORE riconnessione OTB PC [%s]. Num err [%i]\n",modbus_strerror(errno),numerr);
	logvalue(LOG_FILE,errmsg);
      }
      
      if (numerr > 15) {
	system("echo \"Errore di lettura nel registro OTB PC. Programma chiuso\" | /usr/bin/mutt -s \"Errore nella lettura del registro IN OTB PC (74)\" vittorio.giannini@windtre.it");
	myCleanExit("Too many errors while readin OTB PC registers\n");
	exit(EXIT_FAILURE);
      }
    } else {
      //-------------------------------------------------------------------------------------
      if (read_single_state((uint16_t)otb_pc_in[0],OTB_IN8)) {
	logvalue(LOG_FILE,"APERTURA PARZIALE CANCELLO INGRESSO\n");
	if (pulsante(mb,APERTURA_PARZIALE)<0) {
	  logvalue(LOG_FILE,"\tproblemi con pulsante\n");
	}

      }
      //-------------------------------------------------------------------------------------      
      if (read_single_state((uint16_t)otb_pc_in[0],OTB_IN9)) {
	logvalue(LOG_FILE,"APERTURA TOTALE CANCELLO INGRESSO\n");
	if (pulsante(mb,APERTURA_TOTALE)<0) {
	  logvalue(LOG_FILE,"\tproblemi con pulsante\n");
	}
      }
      //-------------------------------------------------------------------------------------  
      numerr=0;
    } // else
    sleep(0.5);
  } // while

  return 0;

}


