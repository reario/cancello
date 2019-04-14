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
#include <modbus.h>
#include "gh.h"

int pulsante(modbus_t *m,int bobina) {
  if ( modbus_write_bit(m,bobina,TRUE) != 1 ) {
    printf("ERRORE DI SCRITTURA:PULSANTE ON");
    return -1;
  }
  sleep(1);
  if ( modbus_write_bit(m,bobina,FALSE) != 1 ) {
    printf("ERRORE DI SCRITTURA:PULSANTE OFF");
    return -1;
  }
  return 0;
}

void logvalue(char *filename, char *message)
{
  /*scrive su filename il messaggio message*/
  FILE *logfile;
  logfile=fopen(filename,"a");
  if(!logfile) return;
  fprintf(logfile,"%s\n",message);
  fclose(logfile);
}


void signal_handler(int sig)
{
  switch(sig) {
  case SIGHUP:
    logvalue(LOG_FILE,"ATT: Reset Modbus connection");
    break;
  case SIGTERM:
    logvalue(LOG_FILE,"terminate signal catched");
    unlink(LOCK_FILE);
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
    logvalue(LOG_FILE,"Cannot Start. There is another process running. exit");
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

  modbus_t *mb;
  modbus_t *mb_otb;
  uint16_t otb_in[10];
  //unit16_t plc_in[10];

  daemonize();

  mb = modbus_new_tcp("192.168.1.157",PORT);
  mb_otb = modbus_new_tcp("192.168.1.11",PORT);
  if ( (modbus_connect(mb) == -1) || (modbus_connect(mb_otb) == -1))
    {
      logvalue("ERRORE non riesco a connettermi con il PLC o OTB");
      exit(1);
    }

  while (1) {
    if (modbus_read_registers(mb_otb, 0, 1, otb_in)<0) {    // leggo lo stato degli ingressi collegati al wireless button
      logvalue(LOG_FILE,"Errore Lettura Registro OTB per Cancello");      
    } else {
      //-------------------------------------------------------------------------------------
      if (read_single_state(otb_in[0],FARI_ESTERNI_IN_SOTTO)) {
	logvalue(LOG_FILE,"APERTURA PARZIALE CANCELLO INGRESSO");
	//  pulsante(mb,APERTURA_PARZIALE)
	pulsante(mb,CICALINO_AUTOCLAVE);
      }
      
      if (read_single_state(otb_in[0],FARI_ESTERNI_IN_SOPRA)) {
	//-------------------------------------------------------------------------------------  
	logvalue(LOG_FILE,"APERTURA TOTALE CANCELLO INGRESSO");
	//  pulsante(mb,APERTURA_TOTALE)
	pulsante(mb,CICALINO_AUTOCLAVE);
	//-------------------------------------------------------------------------------------  
      }
    } // else
  }
  
  modbus_close(mb);
  modbus_free(mb);
  modbus_close(mb_otb);
  modbus_free(mb_otb);
  return 0;

}


