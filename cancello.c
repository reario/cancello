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
//#include "gh.h"

#define CHECK_BIT(var,pos) ((var) & (1<<(pos)))

// IP Device
#define ZBRN1_IP "192.168.1.160"
#define PLC_IP "192.168.1.157"
#define PORT 502

// Uscite PLC cancello
#define APERTURA_PARZIALE 96 /* %M96 */
#define APERTURA_TOTALE 97 /* %M97 */

// Directory
#define RUNNING_DIR     "/home/reario/cancello/"
#define LOCK_FILE       "/home/reario/cancello/cancello.lock"
#define LOG_FILE        "/home/reario/cancello/cancello.log"

modbus_t *mb_plc;
modbus_t *mb_zbrn1;
uint8_t coilmap[] = {APERTURA_PARZIALE,APERTURA_TOTALE};



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

int pulsante(modbus_t *m, uint8_t bobina, uint8_t valore) {
  char errmsg[100];
  
  if ( modbus_write_bit(m,bobina,valore) != 1 ) { // valore = TRUE se da accendere, FALSE se da spegnere
    // The modbus_write_bit() function shall return 1 if successful. 
    // Otherwise it shall return -1 and set errno.
    sprintf(errmsg,"ERRORE DI SCRITTURA:PULSANTE ON %s\n",modbus_strerror(errno));
    logvalue(LOG_FILE,errmsg);
    return -1;
  }
  return 1; 
}

int pulsante_old(modbus_t *m,int bobina) {

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
  logvalue(LOG_FILE,"\tChiudo la connessione con PLC e ZBRN1\n");
  modbus_close(mb_plc);
  modbus_free(mb_plc);
  logvalue(LOG_FILE,"\tLibero la memoria dalle strutture create\n");
  modbus_close(mb_zbrn1);
  modbus_free(mb_zbrn1);
  logvalue(LOG_FILE,"Fine.\n");
  logvalue(LOG_FILE,"****************** END **********************\n");
  
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
  logvalue(LOG_FILE,"****************** START **********************\n");
}

uint8_t operate(modbus_t *m,uint8_t current,uint8_t onoff)
{
  char msg[100];
  uint8_t bobina;
  sprintf(msg,"bit [%u] transizione %s",current,onoff ? "0->1\n":"1->0\n");
  logvalue(LOG_FILE,msg);
  bobina=coilmap[current];
  sprintf(msg,"modbus_write_bit(m,%s,%s);\n",bobina==APERTURA_PARZIALE?"APERTURA_PARZIALE":"APERTURA_TOTALE",onoff?"ON":"OFF");
  // sprintf(msg,"modbus_write_bit(m,%u,%s);\n",bobina,onoff?"ON":"OFF");
  logvalue(LOG_FILE,msg);
  if // (0)
    (modbus_write_bit(m,bobina,onoff) != 1 )
    {
      sprintf(msg,"ERRORE DI SCRITTURA:PULSANTE %s %s\n",onoff ? "0->1\n":"1->0\n",modbus_strerror(errno));
      logvalue(LOG_FILE,msg);
      return -1;
    }  
  return 1;
}


int main (int argc, char ** argv) {

  uint16_t zbrn1_reg[1];
  char errmsg[100];
  uint16_t numerr = 0;
  
  daemonize();
  
  mb_plc = modbus_new_tcp(PLC_IP,PORT);
  mb_zbrn1 = modbus_new_tcp(ZBRN1_IP,PORT);

  uint16_t oldval = 0;
  uint16_t newval = 0;
  
  /* Define a new timeout! */
  uint32_t response_timeout_sec = 2;
  uint32_t response_timeout_usec = 0;

  modbus_set_response_timeout(mb_plc,  response_timeout_sec, response_timeout_usec); // 4 seconds 0 usec
  modbus_set_response_timeout(mb_zbrn1,response_timeout_sec, response_timeout_usec); // 4 seconds 0 usec
  modbus_set_slave(mb_zbrn1,248);
  
  if ( (modbus_connect(mb_plc) == -1 )) {
  sprintf(errmsg,"ERRORE non riesco a connettermi con il PLC %s\n",modbus_strerror(errno));
  //logvalue(LOG_FILE,errmsg);
  myCleanExit(errmsg);
  exit(EXIT_FAILURE);
}
  
  if ( (modbus_connect(mb_zbrn1) == -1)) {
  sprintf(errmsg,"ERRORE non riesco a connettermi con ZBRN1. Premature exit [%s]\n",modbus_strerror(errno));
  //logvalue(LOG_FILE,errmsg);
  myCleanExit(errmsg);
  exit(EXIT_FAILURE);
}

  oldval = newval;
  while (1) {
    
    if ( modbus_read_registers(mb_zbrn1, 0, 1, zbrn1_reg) < 0 ) {    // leggo lo stato degli ingressi collegati al wireless button
      
      numerr++;
      sprintf(errmsg,"ERRORE Lettura Registro ZBRN1 per Cancello [%s]. Num err [%i]\n",modbus_strerror(errno),numerr);
      logvalue(LOG_FILE,errmsg);
    
      modbus_close(mb_zbrn1);      
      modbus_free( mb_zbrn1);
      mb_zbrn1 = modbus_new_tcp(ZBRN1_IP,PORT);
      response_timeout_sec = 4;
      response_timeout_usec = 0;
      modbus_set_response_timeout(mb_zbrn1, response_timeout_sec, response_timeout_usec); 
      modbus_set_slave(mb_zbrn1,248);
      
      if (modbus_connect(mb_zbrn1)==-1) {
	sprintf(errmsg,"\tERRORE riconnessione ZBRN1 [%s]. Num err [%i]\n",modbus_strerror(errno),numerr);
	logvalue(LOG_FILE,errmsg);
      }
      
      if (numerr > 5) {
	system("echo \"Errore di lettura nel registro ZBRN1. Programma chiuso\" | \
                /usr/bin/mutt -s \"Errore nella lettura del registro ZBRN1\" \
                vittorio.giannini@windtre.it"); // installare Mutt
	myCleanExit("Too many errors while reading ZBRN1 registers\n");
	exit(EXIT_FAILURE);
      }
    } else {

#define TOPOLINO
      
#ifdef TOPOLINO
      newval=zbrn1_reg[0]; // registro 1 dove sono messi i primi 16 pulsanti (bit da 0 a 15)
      uint16_t diff = newval ^ oldval;
      if (diff) {
	uint8_t curr;
	for (curr = 0; curr<2; curr++) {
	  if (CHECK_BIT(diff,curr)) {
	    // vedo se il bit curr di oldval è a 0 o a 1
	    // se era a 0 in newval è passato a 1 e viceversa
	    operate(mb_plc, curr, CHECK_BIT(oldval,curr)?FALSE:TRUE);
	  }
	}	
      }
#endif
      
#ifdef PAPERINO
      newval=zbrn1_reg[0]; // registro 1 dove sono messi i primi 16 pulsanti (bit da 0 a 15)
      if (oldval != newval) {
	uint8_t curr;
	for (curr = 0; curr<2; curr++) {
	  if (!CHECK_BIT(oldval,curr) && CHECK_BIT(newval,curr)) {
	    operate(mb_plc,curr,TRUE);
	  }
	  if (CHECK_BIT(oldval,curr) && !CHECK_BIT(newval,curr)) {
	    operate(mb_plc,curr,FALSE);
	  }	  
	}
      }
#endif
#ifdef PLUTO
      newval=zbrn1_reg[0]; // registro 1 dove sono messi i primi 16 pulsanti (bit da 0 a 15)
      if (oldval != newval) {
	uint8_t curr;
	for (curr = 0; curr<2; curr++) {
	  if (!CHECK_BIT(oldval,curr) && CHECK_BIT(newval,curr)) {
	    // 0->1
	    switch ( curr ) {
	    case 0: {
	      // bit posizione 0
	      sprintf(errmsg,"*bit 0 transizione 0->1 %u --> %u [%u]\n",oldval,newval,curr);
	      logvalue(LOG_FILE,errmsg);
	      // connect
	      // modbus_t *mb_plc = modbus_new_tcp(PLC_IP,PORT);
	      // if (modbus_connect(mb_plc) != -1) { 
	      if (pulsante(mb_plc,APERTURA_PARZIALE,TRUE)<0) {
		logvalue(LOG_FILE,"\tproblemi con pulsante\n");
	      } 
	      // close
	      // mobus_close(mb_plc);b
	      // } else {
	      //   sprintf(errmsg,"ERRORE non riesco a connettermi con il PLC %s\n",modbus_strerror(errno));
	      //   logvalue(LOG_FILE,errmsg);
	      // }
	      // mbus_free(mb_plc);
	      break;
	    }
	    case 1: {
	      // bit posizione 1
	      sprintf(errmsg,"bit 1 transizione 0->1 %u --> %u [%u]\n",oldval,newval,curr);
	      logvalue(LOG_FILE,errmsg);
	      if (pulsante(mb_plc,APERTURA_TOTALE,TRUE)<0) {
		logvalue(LOG_FILE,"\tproblemi con pulsante\n");
	      }
	      break;
	    }
	    } 
	  }
	  if (CHECK_BIT(oldval,curr) && !CHECK_BIT(newval,curr)) {
	    // 1->0
	    switch ( curr ) {
	    case 0: {
	      // bit posizione 0
	      sprintf(errmsg,"*bit 0 transizione 1->0 %u --> %u\n",oldval,newval);
	      logvalue(LOG_FILE,errmsg);
	      if (pulsante(mb_plc,APERTURA_PARZIALE,FALSE)<0) {
		logvalue(LOG_FILE,"\tproblemi con pulsante\n");
	      }
	      break;
	    }
	    case 1: {
	      // bit posizione 1
	      sprintf(errmsg,"bit 1 transizione 1->0 %u --> %u\n",oldval,newval);
	      logvalue(LOG_FILE,errmsg);
	      if (pulsante(mb_plc,APERTURA_TOTALE,FALSE)<0) {
		logvalue(LOG_FILE,"\tproblemi con pulsante\n");
	      }
	      break;
	    }
	    } // switch
	  } // if (CHECK_BIT()......
	} // for ( curr ) { .......
      }
#endif
    numerr=0;
    oldval=newval;                 
  } // else
    usleep(100000);
  } // while
  
  return 0;
  
}


