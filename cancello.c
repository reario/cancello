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

#define CHECK_BIT(var,pos) ( (var) & (1<<(pos)) )

// TWIDO IP Device
#define ZBRN1_IP "192.168.1.160"
#define PLC_IP "192.168.1.157"
#define PORT 502
// Uscite PLC TWIDO
#define APERTURA_PARZIALE 96 /* %M96 TWIDO */
#define APERTURA_TOTALE 97 /* %M97 TWIDO */
#define SERRATURA_PORTONE 12 /* %M12 TWIDO */
#define LUCI_STUDIO_SOTTO 7 /* %M7 */

// OTB IP Device
#define OTB_IP "192.168.1.11"
// IN/OUT OTB
#define OTB_OUT 100 // registro uscite OTB
#define FARI_ESTERNI_SOPRA 0 // bit 0 del registro OUT_OTB
#define FARI_ESTERNI_SOTTO 1 // bit 1 del registro OUT_OTB
#define OTB_IN 0 // registro ingressi OTB
#define FARI_ESTERNI_IN_SOPRA 11 // bit 11 registro IN_OTB 
#define FARI_ESTERNI_IN_SOTTO 10 // bit 10 registro IN_OTB 

// Directory
#define RUNNING_DIR     "/home/reario/cancello/"
#define LOCK_FILE       "/home/reario/cancello/cancello.lock"
#define LOG_FILE        "/home/reario/cancello/cancello.log"

modbus_t *mb_zbrn1;
uint8_t coilmap[] = {APERTURA_PARZIALE,APERTURA_TOTALE,SERRATURA_PORTONE,LUCI_STUDIO_SOTTO,255,255};

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

void myCleanExit(char *from) {
  logvalue(LOG_FILE,from);
  unlink(LOCK_FILE);
  logvalue(LOG_FILE,"\tChiudo la connessione con PLC e ZBRN1\n");
  //modbus_close(mb_plc);
  modbus_close(mb_zbrn1);
  logvalue(LOG_FILE,"\tLibero la memoria dalle strutture create\n");
  //modbus_free(mb_plc);
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

// serve per i faretti, ripresa da newf.c dentro ~/faretti
int faretti(uint16_t FARI) {

  modbus_t *mb_otb;
  char errmsg[100];

  // IMPORTANTE: ormask e and mask sono costruite tenendo conto che:
  //  - nella and_mask ci sono 0 per i bit che cambiano e 1 per quelli che non cambiano
  //    passando la diff tra quelli della interazione prima e quelli della interazione dopo
  //    la and_mask la ottengo facendo la negazione dei coils
  //  - nella or_mask c'è 1 se il bit va da 0->1 e c'è 0 se il bit va da 1->0
  //  - nella or_mask contano solo i bit che cambiano. Gli altri bit sono ininfluenti
  //  - AND_MASK: per ogni bit che cambia metto 1: (1<<BITa)|(1<<BITb)|.....(BITn) e poi faccio
  //    la negazione trovandomi 0 dove cambiano e 1 dove rimangono invariati
  //  - OR_MASK: se 0->1 (1<<BITa), se 1->0 (0<<BITb) facendo l'OR di tutti

  uint16_t and_mask = 0;
  uint16_t or_mask  = 0;
  uint16_t status[1];

  mb_otb = modbus_new_tcp(OTB_IP,PORT);
  modbus_set_response_timeout(mb_otb,  2, 0); // 2 seconds 0 usec
  if ( (modbus_connect(mb_otb) == -1 )) {
    sprintf(errmsg,"ERRORE non riesco a connettermi con OTB %s\n",modbus_strerror(errno));
    logvalue(LOG_FILE,errmsg);
    return -1;
  }
    
  // 1) check lo stato dei faretti
  if (modbus_read_registers(mb_otb, OTB_IN, 1,status) == -1)
    {
      sprintf(errmsg,"Errore READ IN registro of OTB\n");
      logvalue(LOG_FILE,errmsg);
      return -1;
    }

  // se CHECK_BIT=1 allora il faretto è acceso e lo devo spegnere (or_mask con 0<<FARI)
  // se CHECK_BIT=0 allora il faretto è spento e lo devo accender (or_mask con 1<<FARI)
  uint16_t accendo_o_spengo=CHECK_BIT(status[0],FARI==FARI_ESTERNI_SOPRA?FARI_ESTERNI_IN_SOPRA:FARI_ESTERNI_IN_SOTTO)?0:1;
  and_mask = ~(1<<FARI);
  or_mask = (accendo_o_spengo<<FARI);
  
  /*
  if (CHECK_BIT(status[0],FARI==FARI_ESTERNI_SOPRA?FARI_ESTERNI_IN_SOPRA:FARI_ESTERNI_IN_SOTTO)) {
    // fari esterni sotto accesi: allora li spengo
    sprintf(errmsg,"Spengo i fari esterni \n");
    logvalue(LOG_FILE,errmsg);

    and_mask = ~(1<<FARI);
    or_mask = (0<<FARI);
  } else {
    // fari esterni sopra erano spenti: allora li accendo
    sprintf(errmsg,"Accendo i fari esterni \n");
    logvalue(LOG_FILE,errmsg);
    
    and_mask = ~(1<<FARI);
    or_mask = (1<<FARI);
  }
  */
  
  if (modbus_mask_write_register(mb_otb,OTB_OUT,and_mask,or_mask) == -1) {
    sprintf(errmsg,"ERRORE nella funzione interruttore %s\n",modbus_strerror(errno));
    logvalue(LOG_FILE,errmsg);
    modbus_close(mb_otb);
    modbus_free(mb_otb);
    return -1;
  }
  
  modbus_close(mb_otb);
  modbus_free(mb_otb);
  
  return 1;

  /* printbitssimple(R); */
  /* R = (R & and_mask) | (or_mask & (~and_mask)); */
  /* printf("dopo R = "); */
  /* printbitssimple(R); */
}

uint8_t cancello(uint8_t current, uint8_t onoff)
{
  modbus_t *mb_plc;
  char msg[100];
  uint8_t bobina;

  mb_plc = modbus_new_tcp(PLC_IP,PORT);
  modbus_set_response_timeout(mb_plc,  2, 0); // 2 seconds 0 usec
  if ( (modbus_connect(mb_plc) == -1 )) {
      sprintf(msg,"ERRORE non riesco a connettermi con il PLC %s\n",modbus_strerror(errno));
      return -1;
  }
  sprintf(msg,"bit [%u] transizione %s",current,onoff ? "0->1\n":"1->0\n");
  logvalue(LOG_FILE,msg);
  bobina=coilmap[current];
  sprintf(msg,"modbus_write_bit(m,%u,%s);\n",bobina,onoff?"ON":"OFF");
  logvalue(LOG_FILE,msg);
  if (current >= 4) {
    modbus_close(mb_plc);
    modbus_free(mb_plc);
    return 1;
  }
  if (modbus_write_bit(mb_plc,bobina,onoff) != 1 )
    {
      sprintf(msg,"ERRORE DI SCRITTURA:PULSANTE %s %s\n",onoff ? "0->1\n":"1->0\n",modbus_strerror(errno));
      logvalue(LOG_FILE,msg);
      modbus_close(mb_plc);
      modbus_free(mb_plc);
      return -1;
    }
  modbus_close(mb_plc);
  modbus_free(mb_plc);
  return 1;
}

int main (int argc, char ** argv) {

  uint16_t zbrn1_reg[1];
  char errmsg[100];
  uint16_t numerr = 0;
  
  daemonize();
  
  //mb_plc = modbus_new_tcp(PLC_IP,PORT);
  mb_zbrn1 = modbus_new_tcp(ZBRN1_IP,PORT);

  uint16_t oldval = 0;
  uint16_t newval = 0;
  
  /* Define a new timeout! */
  uint32_t response_timeout_sec = 2;
  uint32_t response_timeout_usec = 0;

  //modbus_set_response_timeout(mb_plc,  response_timeout_sec, response_timeout_usec); // 4 seconds 0 usec
  modbus_set_response_timeout(mb_zbrn1,response_timeout_sec, response_timeout_usec); // 4 seconds 0 usec
  modbus_set_slave(mb_zbrn1,248);
  if ( (modbus_connect(mb_zbrn1) == -1)) {
  sprintf(errmsg,"ERRORE non riesco a connettermi con ZBRN1. Premature exit [%s]\n",modbus_strerror(errno));
  //logvalue(LOG_FILE,errmsg);
  myCleanExit(errmsg);
  exit(EXIT_FAILURE);
}

  oldval = newval;
  while (1) {
    
    if ( modbus_read_registers(mb_zbrn1, 0, 1, zbrn1_reg) < 0 ) {
      // leggo lo stato degli ingressi collegati al wireless button
      numerr++;
      	if ( numerr > 1  ) {
	  sprintf(errmsg,"ERRORE Lettura Registro ZBRN1 per Cancello [%s]. Num err [%i]\n",modbus_strerror(errno),numerr);
	  logvalue(LOG_FILE,errmsg);
	}

      modbus_close(mb_zbrn1);
      modbus_free( mb_zbrn1);
      mb_zbrn1 = modbus_new_tcp(ZBRN1_IP,PORT);
      modbus_set_response_timeout(mb_zbrn1, response_timeout_sec, response_timeout_usec); 
      modbus_set_slave(mb_zbrn1,248);
      if (modbus_connect(mb_zbrn1)==-1) {

	  sprintf(errmsg,"\tERRORE Riconnessione ZBRN1 [%s]. Num err [%i]\n",modbus_strerror(errno),numerr);
	  logvalue(LOG_FILE,errmsg);
	
      }
      if (numerr > 15) {
	system("echo \"Errore di lettura nel registro ZBRN1. Programma chiuso\" | \
                /usr/bin/mutt -s \"Errore nella lettura del registro ZBRN1\" \
                vittorio.giannini@windtre.it"); // installare Mutt
	myCleanExit("Too many errors while reading ZBRN1 registers\n");
	exit(EXIT_FAILURE);
      }
    } else {
      newval=zbrn1_reg[0]; // registro 1 dove sono messi i primi 16 pulsanti (bit da 0 a 15)
      uint16_t diff = newval ^ oldval;
      if (diff) {
	uint8_t curr;
	for (curr = 0; curr<6; curr++) {
	  /* 
	     al momento solo 5 pulsanti:
	     2 pulsanti per cancello scorrevole (apertura totale e apertura parziale) #0 #1
	     1 pulsante per portoncino #2
	     1 pulsante per luce studio sotto #3 
	     1 pulsante scatola quadrata #4 #5 (attivo al rilascio)
	  */
	  // diff contiene 1 sui bit cambiati
	  // se ho 1 in un bit di oldval significa 1->0
	  // se ho 0 in un bit di oldval significa 0->1
	  if (CHECK_BIT(diff,curr)) {
	    // vedo se il bit curr di oldval è a 0 o a 1
	    if (curr<4) {
	      cancello(curr, CHECK_BIT(oldval,curr) ? FALSE : TRUE);
	    } else { // !CHECK_BIT(oldval,curr) vuol dire che ho la transizione da 0->1
	      if ( (curr == 4) && !CHECK_BIT(oldval,curr)) {
		faretti(FARI_ESTERNI_SOTTO);
	      }
	      if ( (curr == 5) && !CHECK_BIT(oldval,curr)) {
		faretti(FARI_ESTERNI_SOPRA);
	      }	      
	    }	      
	  }
	}	
      }
      numerr=0;
      oldval=newval;                 
    } // else
    usleep(200000); // 2/10 sec
  } // while
  return 0; 
}
