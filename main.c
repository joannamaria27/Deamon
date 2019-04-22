#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <signal.h>
#include <sys/syslog.h>
#include <sys/mman.h>
#include <stdbool.h>
#include <time.h>
#include <utime.h>
#include <assert.h>


void kopiowaie(char * plikZrodlowy, char * plikDocelowy)
{
    int plikZ = open(plikZrodlowy,O_RDONLY); 
    int plikD = open(plikDocelowy, O_CREAT |O_TRUNC| O_RDWR  ,777);
	if(plikZ<0)
	{
	    printf ("błąd: "); 
        exit(0);
    }
    int ileOdcz=0;
    int ileZapis=0;
    char buffor[200];
    do
    {   
        memset(buffor,0,sizeof(buffor));
        ileOdcz=read(plikZ,buffor,sizeof(buffor));    
    }
    while(ileOdcz>=197);
    close(plikZ);
    ileZapis=write(plikD,buffor,ileOdcz);
    close(plikD);
    syslog(LOG_INFO,"plik %s został skopiowany poprzez mapowanie",plikZrodlowy);
}

void kopiowanie_mmap(char *sciezkaZ, char *sciezkaD){
    
    int plikZ, plikD;
    char *zr, *doc;
    //struct stat s;
    size_t rozmiarPliku;

    /* mmap(void *start, size_t length, int prot, int flags, int fd, off_t offset);
    start - określa adres, w którym chcemy widzieć odwzorowanie pliku. Nie jest wymagane i nie zawsze jest przestrzegane
    przez system operacyjny.
    length - liczba bajtów jaką chcemy odwzorować w pamięci.
    prot - flagi określające uprawnienia jakie chcemy nadać obszarowi pamięci, np. tylko do odczytu, etc.
    flags - dodatkowe flagi określające sposób działania wywołania mmap.
    fd - deskryptor pliku, który chcemy odwzorować w pamięci.
    offset - liczba określająca od którego miejsca w pliku chcemy rozpocząć odwzorowywanie.*/

    plikZ = open(sciezkaZ, O_RDONLY);
    
    if(sciezkaZ<0)
	{
	    printf ("błąd: %s ", strerror (errno)); 
	    exit(1);
   	}
    rozmiarPliku = lseek(plikZ, 0, SEEK_END);
    zr = mmap(NULL, rozmiarPliku, PROT_READ, MAP_PRIVATE, plikZ, 0);

    plikD = open(sciezkaD, O_RDWR | O_CREAT, 777);
    ftruncate(plikD, rozmiarPliku);
    doc = mmap(NULL, rozmiarPliku, PROT_READ | PROT_WRITE, MAP_SHARED, plikD, 0);

    /*mmap, munmap - mapowanie lub usunięcie mapowania plików lub urządzeń w pamięci

    memcpy (void* dest, const void* src, size_t size);
    dest - wskaźnik do obiektu docelowego.
    src - wskaźnik do obiektu źródłowego.
    size - liczba bajtów do skopiowania.

    munmap(void *start, size_t length);*/

    memcpy(doc, zr, rozmiarPliku);
    munmap(zr, rozmiarPliku);
    munmap(doc, rozmiarPliku);
    syslog(LOG_INFO,"plik %s został skopiowany poprzez mapowanie",sciezkaZ);

    close(plikZ);
    close(plikD);
}

int CzyKatalog(char* path)
{
    struct stat info;       
    if((stat(path, &info)) < 0)
        return -1;
    else 
        return info.st_mode;
}


typedef struct Pliki
{
    char nazwaPliku[60];
    char dataPliku[60];
    float rozmiarPliku;
    struct Pliki * nastepny;
}Pliki;

void dodawanie(Pliki ** glowa, char nazwa[], char data[], float rozmiar)
{
    Pliki *p,*e;
    e=(Pliki*)calloc(1,sizeof(Pliki));
    e->nastepny=NULL;
    strcpy(e->nazwaPliku,nazwa);
    strcpy(e->dataPliku,data);
    e->rozmiarPliku=rozmiar;
    p=*glowa;
    if(p!=NULL)
    {
        while(p->nastepny!=NULL)
            p=p->nastepny;
        p->nastepny=e;        
    }
    else
        *glowa=e;
}

void wypiszListe(Pliki* lista)
{
    Pliki* wsk = lista;

    if(lista == NULL)
        printf("LISTA JEST PUSTA");
    else
    {
        int i = 1;
        while( wsk != NULL)
        {
            printf("%d %s %s %f \n", i, wsk->nazwaPliku, wsk->dataPliku, wsk->rozmiarPliku);
            wsk=wsk->nastepny;
            i++;
        }
    }
}

bool czyIstnieje(Pliki * pZr, char * nazwaP)
{   
        while(pZr!=NULL)
        {            
            if(strcmp(pZr->nazwaPliku,nazwaP)==0)
            {              
                return true;                                  
            } 
            else
            {       
                if(pZr->nastepny==NULL)                    
                    return false;                   
            } 
            pZr=pZr->nastepny;       
        }
 
}

void zmiana_daty(char *plikZ, char *plikD)
{
    struct stat filestat;
    struct utimbuf nowa_data; // to specify new access and modification times for a file
    
    stat(plikZ, &filestat);
   // nowa_data.actime = filestat.st_atim.tv_sec;
   // nowa_data.modtime = filestat.st_mtim.tv_sec;
    nowa_data.modtime = filestat.st_mtime;
    nowa_data.actime = filestat.st_atime;
    
    utime(plikD, &nowa_data);   // zmienia czas dostepu i modyfikacji pliku
    chmod(plikD, filestat.st_mode);

}


void dodawaniePlikow(char * argument, Pliki * pliki)
{
    struct dirent *plik; //wskazuje na element w katalogu; przechowuje rózne informacje
    DIR * sciezka; //reprezentuje strumień sciezki

    char sc[50];
    char t[100];
    char * ar1;
    float rozmiar;

    if((sciezka = opendir (argument))!=NULL) //otwiera strumień do katalogu
    {        
        while((plik = readdir (sciezka))!=NULL)  //zwraca wskaznik do struktury reprez. plik
        {
            strcpy(sc,argument);
            strcat(sc,"/");
            ar1=strcat(sc,plik->d_name);
            struct stat sb;
            stat(ar1, &sb);
            rozmiar=sb.st_size;
            strftime(t, 100, "%d/%m/%Y %H:%M:%S", localtime( &sb.st_mtime));
            if (!S_ISREG(sb.st_mode)) 
            {
                continue;
            } 
            else
            {
                dodawanie(&pliki, plik->d_name, t, rozmiar);
            }
            
        }
        closedir(sciezka); //zamyka strumien sciezki
    }
    else
    {
        printf("Bład otwarcia podanej scieżki zrodlowej\n");
        exit(1);
    } 
}

volatile int sygnal = 1;
void handler(int signum)
    {
        syslog(LOG_INFO,"Demon został obudzony po odebraniu sygnału");
    }

int main(int argc, char **argv)  
{
    setlogmask (LOG_UPTO (LOG_INFO));
    openlog ("demon-projekt", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);

    if(argc<=1)
    {
        printf("Brak argumentów\n");
        syslog(LOG_INFO,"Brak argumentów");
        return -1;
    }
    if(argc>6)
    {
        printf("Za dużo argumentów\n");
        syslog(LOG_INFO,"Za dużo argumentów");
        return -1;
    }
     if((CzyKatalog(argv[2])==1) && (CzyKatalog(argv[1])==1))
    {
        printf("Scieżka zródłowa i docelowa nie jest katalogiem\n");
        syslog(LOG_INFO,"Scieżka zródłowa i docelowa nie jest katalogiem");
        return -1;
    }
    if(CzyKatalog(argv[1])==1)
    {
        printf("Scieżka źródłowa nie jest katalogiem\n");
        syslog(LOG_INFO,"Scieżka źródłowa nie jest katalogiem");
        return -1;
    }
    if(CzyKatalog(argv[2])==1)
    {
        printf("Scieżka docelowa nie jest katalogiem\n");
        syslog(LOG_INFO,"Scieżka docelowa nie jest katalogiem");
        return -1;
    }
	
   
    double czas=5; //*60 =>minuty
    if(argc==4)
    {
        czas=atof(argv[3]);
    }

    double rozmiarDzielacyPliki=0;
    if (argc==5)
    {
        czas=atof(argv[3]);
        rozmiarDzielacyPliki=atof(argv[4]);
    }

    if(argc==6 && argv[5][0] == '-'  && argv[5][1] == 'R')
    {
        czas=atof(argv[3]);
        rozmiarDzielacyPliki=atof(argv[4]);
       
        //rekurencyjna synchronizacja katalogów
        
    }
    syslog (LOG_NOTICE, "Demon śpi");
    sleep(czas);
    syslog (LOG_INFO, "Demon został obudzony po %.2f minutach",czas);

    	
    Pliki *plikiZr;
    Pliki *plikiDoc;
    plikiZr=(Pliki*)calloc(1,sizeof(Pliki)); //pierwszy el zawsze pusty
    plikiDoc=(Pliki*)calloc(1,sizeof(Pliki)); //pierwszy el zawsze pusty
	
    signal(SIGUSR1, handler);
	
    dodawaniePlikow(argv[1], plikiZr);
    dodawaniePlikow(argv[2], plikiDoc);
	
	
	//wypisanie listy
printf("-------------------------------------------------\n");
wypisz_liste(plikiZr);
printf("-------------------------------------------------\n");
wypisz_liste(plikiDoc);

// signal(SIGUSR1, ignor);//ignoruje demona

Pliki * plikiZrKopia;
Pliki * plikiDocKopia;
plikiZrKopia = plikiZr->nastepny;
plikiDocKopia = plikiDoc->nastepny;
int sk=0;


    while(plikiZr!=NULL)
    {   
        plikiDoc=plikiDocKopia;
        sk=0;        
        while((plikiDoc!=NULL) && (sk==0))
        {             
            if(strcmp(plikiZr->nazwaPliku,plikiDoc->nazwaPliku)==0)
            {
                if(strcmp(plikiZr->dataPliku,plikiDoc->dataPliku)!=0)
                {
                    char sc6[50];
                    char sc7[50];
                    strcpy(sc6,argv[1]);
                    strcat(sc6,"/");
                    strcat(sc6,plikiZr->nazwaPliku);
                    strcpy(sc7,argv[2]);
                    strcat(sc7,"/");
                    strcat(sc7,plikiDoc->nazwaPliku);                    
                     if(argc==5)
                    {
                        if(rozmiarDzielacyPliki>=plikiZr->rozmiarPliku)
                        {
                            kopiowanie_mmap(sc6,sc7);
                        }
                        else
                        {
                            kopiowanie(sc6,sc7);
                            zmianaDaty(sc6,sc7);
                        }                        
                    }   
                    else
                    {
                        kopiowanie(sc6,sc7);
                        zmianaDaty(sc6,sc7);
                        
                        //break;
                    }
                    sk=1;          
                }                                
            } 
            else
            {

                if(plikiDoc->nastepny==NULL && sk==0)
                {
                    char sc3[50];
                    char sc5[50];
                    strcpy(sc3,argv[1]);
                    strcat(sc3,"/");
                    strcat(sc3,plikiZr->nazwaPliku);
                    strcpy(sc5,argv[2]);
                    strcat(sc5,"/");
                    strcat(sc5,plikiZr->nazwaPliku);
                    kopiowaie(sc3,sc5);
                }
            }   
            plikiDoc=plikiDoc->nastepny;   
        }
        plikiZr=plikiZr->nastepny;
    }
	
	
    plikiDoc=plikiDocKopia;
    plikiZr=plikiZrKopia;
    while(plikiDoc!=NULL)
    {
        bool cz=czyIstnieje(plikiZr,plikiDoc->nazwaPliku);
        if(cz==false)
        {
            char sc4[50];
            strcpy(sc4,argv[2]);
            strcat(sc4,"/");
            strcat(sc4,plikiDoc->nazwaPliku);
            remove(sc4);
            syslog(LOG_INFO,"plik %s został usnięty", plikiDoc->nazwaPliku);
        }
        plikiDoc=plikiDoc->nastepny;
    } 


    signal(SIGUSR1, handler);
	 syslog (LOG_NOTICE, "Demon działa – synchronizuje");
        sleep(60);




    closelog();

}
