/* 
per server all'esterno di GH 
*/

#define HOST "giannini.dynu.com"
#define PORT 502
//#define HOST 192.168.1.157
/*
per server dentro GH
 */

/*
#define HOST "twido"
#define PORT 502
*/

#define _DB_
/*#define _PLOT_*/

#define RUNNING_DIR     "/home/reario/cancello/"
#define LOCK_FILE       "/home/reario/cancello/cancello.lock"
#define LOG_FILE        "/home/reario/cancello/cancello.log"
/*---------------------*/
#define GIORNO 0
#define MESE 1
#define ANNO 2
#define ORA 3


/* INPUT */
/* INGRESSI: NOME bit della word %MW65 del PLC 
 AUTOCLAVE 0
 POMPA_SOMMERSA 1
 RIEMPIMENTO 2
 LUCI_ESTERNE_SOTTO 3
 CENTR_R8 4
 LUCI_GARAGE_DA_4 5
 LUCI_GARAGE_DA_2 6
 LUCI_TAVERNA_1_di_2 7
 LUCI_TAVERNA_2_di_2 8
 INTERNET 9
 C9912 10
 LUCI_CUN_LUN 11
 LUCI_CUN_COR 12
 LUCI_STUDIO_SOTTO 13
 LUCI_ANDRONE_SCALE 14
 GENERALE_AUTOCLAVE 15
 LUCI_CANTINETTA 16
*/


/*                     |<-  IN ->|OUT |                                                               |<-                  OTB                    ->|
  ---------------------------------------------------------------------------------------------------------------------------------------------------
  |<- PM9 regis ->| HH |IN0 |IN1 |OUT |#Au |#Po | #Au/h | #Po/h |#Se/h Au|#Se/h Po|#Se/gi Au|#Se/gi Po| OTB DIN  | bar | bar | DOUT | AOUT1 | AOUT2 | mean 
  ---------------------------------------------------------------------------------------------------------------------------------------------------
  | 7 |......| 63 | 64 | 65 | 66 | 67 | 68 | 69 | 70    | 71    | 72     | 73     | 74      | 75      | 507      | 508 | 509 | 707  |  708  | 709   | PLC%MW
  ---------------------------------------------------------------------------------------------------------------------------------------------------
  | 0 |......| 56 | 57 | 58 | 59 | 60 | 61 | 62 | 63    | 64    | 65     | 66     | 67      | 68      | 69       | 70  | 71  |      |       |       | TA_REG
  ---------------------------------------------------------------------------------------------------------------------------------------------------

 DISTRIBUZIONE ORARIA DEGLI SCATTI E DEI SECONDI DI ACCENSIONE
  ---------------------------------------------------------------------------------------
  |<-scat./h aut.->|<-scat./h poz.->| MPLC| Res.| HPLC |<-seco./h aut.->|<-seco./h poz.->| meaning 
  ----------------------------------------------------------------------------------------
  | 100 |....| 123 | 124 |....| 147 | 148 | 149 | 150  | 151 |....| 174 | 175 |....| 198 | PLC %MW
  ----------------------------------------------------------------------------------------
  |  0  |....| 23  | 24  |....| 47  | 48  |  49 |  50  | 51  |....|  74 |  75 |....|  98 | TAB_SCATTI_SECONDI
  ----------------------------------------------------------------------------------------
  |<-----DISTRIBUZIONE SCATTI------>|     |xxxxx|      |<-----DISTRIBUZIONE SECONDI----->|
  ----------------------------------------------------------------------------------------

  ------------------------
  INPUT:
  %MW65 %I0.0:16 IN 0-15
  %MW66 %I0.16:8 IN 16-23
  -----------------------
  OUTPUT:
  %MW67 %Q0.0:15 OUT 0-15
  -----------------------
  COIL:
  LUCI_ESTERNE_SOTTO 2
  LUCI_CUN_LUN 3 
  LUCI_CUN_COR 4 
  LUCI_GARAGE 5 
  LUCI_TAVERNA 6
  LUCI_STUDIO_SOTTO 7
  LUCI_ANDRONE_SCALE 8
  LUCI_CANTINETTA 10 
  -----------------------
  */


#define NUMINPUT 17 /* input attualmente usati */
struct item {
  char tag;
  char name[30];
  char stato[4];
  char statoprec[4];
};

struct item input[]={ {' ', "Autoclave"},
		      {' ', "Pompa po."},
		      {' ', "Riempimento serbatoio"},
		      {'C', "Luci esterne"},
		      {' ', "Centr. R8"},
		      {'G', "Luce garage da 4"},
		      {'G', "Luce garage da 2"},
		      {'F', "Luce taverna 1"},
		      {'F', "Luce taverna 2"},
		      {' ', "INTERNET"},
		      {' ', "9912"},
		      {'D', "Luce cunicolo lungo"},		      
		      {'E', "Luce cunicolo corto"},
		      {'I', "Luce studio sotto"},
		      {'H', "Luce androne scale"},		      
		      {' ', "Generale autoclave"},
		      {'M', "Luce cantinetta"}
};

/* TAB_REG */
#define IN0 58 /* primi 16 input */
#define IN1 59 /* rimanenti 8 input */
#define OUT 60 /* registro che contiene le varie %Q0.x */
#define NAU 61 /* Num scatti Autoclave totali giornalieri*/
#define NPO 62 /* Num scatti Pozzo totali giornalieri*/
#define SHA 63 /* Scatti nell'ora Autoclave */
#define SHP 64 /* Scatti nell'ora Pozzo */
#define SEHA 65 /* Secondi di accensione nell'ora corrente Autoclave*/  
#define SEHP 66 /* Secondi di accensione nell'ora corrente Pozzo*/  
#define SEGA 67 /* Secondi di accensione giornalieri Autoclave*/  
#define SEGP 68 /* Secondi di accensione giornalieri Pozzo*/
  
/* TAB_SCATTI */
#define HPLC 48 /* ora PLC */
#define MPLC 50 /* minuti PLC */

/* BOBINE DEI PULSANTI COMANDABILI DA ESTERNO: MAGELIS, INTERFACCIA A CARATTERI */
#define LUCI_ESTERNE_SOTTO 2 /* %M2 */
#define LUCI_CUN_LUN 3 /* %M3 */
#define LUCI_CUN_COR 4 /* %M4 */
#define LUCI_TAVERNA 5 /* %M5 */
#define LUCI_GARAGE 6 /* %M6 */
#define LUCI_STUDIO_SOTTO 7 /* %M7 */
#define LUCI_ANDRONE_SCALE 8 /* %M8 */
#define LUCI_CANTINETTA 10 /* %M10 */
#define SERRATURA_PORTONE 12 /* %M12 */
#define APERTURA_PARZIALE 96 /* %M96 */
#define APERTURA_TOTALE 97 /* %M97 */
/* OTB USCITE registro posizione 0 a partire dal registro 100 */ 
#define FARI_ESTERNI_SOPRA 0 /* 0-esimo bit dell'uscita dell'OTB Q0 */
#define FARI_ESTERNI_SOTTO 1 /* 1-esimo bit dell'uscita dell'OTB Q1 */
#define OTB_Q2 2 /* 2-esimo bit dell'uscita dell'OTB Q2 */
#define OTB_Q3 3 /* 3-esimo bit dell'uscita dell'OTB Q3 */
#define OTB_Q4 4 /* 4-esimo bit dell'uscita dell'OTB Q4 */
#define OTB_Q5 5 /* 5-esimo bit dell'uscita dell'OTB Q5 */
#define OTB_Q6 6 /* 6-esimo bit dell'uscita dell'OTB Q6 */
#define OTB_Q7 7 /* 7-esimo bit dell'uscita dell'OTB Q7 */
/* OTB INGRESSI registro posizione 0 a partire dal registro 0 */
#define FARI_ESTERNI_IN_SOPRA 11 /* 11-esimo bit dell'ingresso dell'OTB IN11*/
#define FARI_ESTERNI_IN_SOTTO 10 /* 10-esimo bit dell'ingresso dell'OTB IN10*/
#define OTB_IN9 9 /* 9-esimo bit dell'ingresso dell'OTB IN09 */
#define OTB_IN8 8 /* 8-esimo bit dell'ingresso dell'OTB IN08 */
#define OTB_IN7 7 /* 7-esimo bit dell'ingresso dell'OTB IN07 */
#define OTB_IN6 6 /* 6-esimo bit dell'ingresso dell'OTB IN06 */
#define OTB_IN5 5 /* 5-esimo bit dell'ingresso dell'OTB IN05 */
#define OTB_IN4 4 /* 4-esimo bit dell'ingresso dell'OTB IN04 */
#define OTB_IN3 3 /* 3-esimo bit dell'ingresso dell'OTB IN03 */
#define OTB_IN2 2 /* 2-esimo bit dell'ingresso dell'OTB IN02 */
#define OTB_IN1 1 /* 1-esimo bit dell'ingresso dell'OTB IN01 */
#define OTB_IN0 0 /* 0-esimo bit dell'ingresso dell'OTB IN00 */

/* Cicalini */
#define CICALINO_AUTOCLAVE 60 /* %M60 */
#define CICALINO_POMPA_POZZO 61 /* %M61 */
/*========================================*/


/*========================================*/

/*========================================*/

void usage();
uint16_t read_state(uint16_t reg, uint16_t q);
uint16_t read_single_state(uint16_t reg, uint16_t q);
uint16_t set_state(uint16_t reg, uint16_t q);
uint16_t reset_state(uint16_t reg, uint16_t q);
uint16_t invert_state(uint16_t reg, uint16_t q);
void printbitssimple(uint16_t n);

