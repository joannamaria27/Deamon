#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/types.h>
#include <dirent.h>
#include <syslog.h>
#include <signal.h>
#include <sys/syslog.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <time.h>
#include <utime.h>
#include <stdbool.h>
#include <getopt.h>


int rekSynchro(char *sciezkaZ, char *sciezkaD, bool rekurencja, long int rozmiar)
{
    DIR *plikZ;
    DIR *plikD;
    struct dirent *plik;
    char srcpath[100];
    char dstpath[100];
    struct stat srcfileinfo;
    struct stat dstfileinfo;
    if (((plikZ = opendir(sciezkaZ)) == NULL) || ((plikD = opendir(sciezkaD)) == NULL))
    {
        syslog(LOG_ERR,"Blad otwarcia katalogu\n");
        return -1;
    }
    syslog(LOG_INFO,"synchronizacja dwóch katalogów %s %s",sciezkaZ,sciezkaD);
    while ((plik = readdir(plikZ)) != NULL)
    {
        if( (strcmp(plik->nazwaPliku,".")==0) || (strcmp(plik->nazwaPliku,"..")==0) ) continue;
        dstfileinfo.st_mtime = 0;
        strcpy(srcpath,sciezkaZ);
        strcpy(dstpath,sciezkaD);
        strcat(srcpath,"/");
        strcat(srcpath,plik->nazwaPliku);
        strcat(dstpath,"/");
        strcat(dstpath,plik->nazwaPliku);
        stat(srcpath,&srcfileinfo);
        stat(dstpath,&dstfileinfo);
        
        if(S_ISDIR(srcfileinfo.st_mode)) {
                if (rekurencja == true) //jeśli użytkownik wybral rekurencyjną synchronizacje
                {
                    if (stat(dstpath,&dstfileinfo) == -1) //jeśli w katalogu docelowym brak folderu z katalogu źródłowego
                    {
                        mkdir(dstpath,srcfileinfo.st_mode); //utworz w katalogu docelowym folder
                        rekSynchro(srcpath,dstpath,rekurencja,rozmiar); //przekopiuj do niego pliki z folderu z katalogu źródłowego
                    }
                    else rekSynchro(srcpath,dstpath,rekurencja,rozmiar);
                }
        }
    }

    closedir(plikD);
    closedir(plikZ);
    free(plik);
    return 0;
}


void kopiowanie(char * plikZrodlowy, char * plikDocelowy)
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
    char buffor[1024];
    do
    {   
        memset(buffor,0,sizeof(buffor));
        ileOdcz=read(plikZ,buffor,sizeof(buffor));    
    }
    while(ileOdcz>=1024);
    close(plikZ);
    ileZapis=write(plikD,buffor,ileOdcz);
    syslog(LOG_INFO,"plik %s został skopiowany poprzez zwykłe kopiowanie",plikZrodlowy);
    close(plikD);
   
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
    rozmiarPliku = lseek(plikZ, 0, SEEK_END); //////zmi
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

bool czyIstnieje(Pliki * p, char * nazwaP)
{   
        while(p!=NULL)
        {            
            if(strcmp(p->nazwaPliku,nazwaP)==0)
            {              
                return true;                                  
            } 
            else
            {       
                if(p->nastepny==NULL)                    
                    return false;                   
            } 
            p=p->nastepny;       
        }
}

void zmianaDaty(char *plikZ, char *plikD)
{

    struct stat filestat;
    struct utimbuf nowa_data; // to specify new access and modification times for a file
    
    stat(plikZ, &filestat);
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
        printf("Bład otwarcia podanej scieżki\n");
        exit(1);
    } 
}

volatile int sygnal = 1; //czy potrzebne
void handler(int signum)
    {
        syslog(LOG_INFO,"Demon został obudzony po odebraniu sygnału");
    }


int main(int argc,char** argv)
{
    setlogmask (LOG_UPTO (LOG_INFO));
    openlog ("demon-projekt", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);
    
    char * sciezkaZrodlowa= argv[1];
    char * sciezkaDocelowa= argv[2];

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
     if((CzyKatalog(sciezkaDocelowa)==1) && (CzyKatalog(sciezkaZrodlowa)==1))
    {
        printf("Scieżka zródłowa i docelowa nie jest katalogiem\n");
        syslog(LOG_INFO,"Scieżka zródłowa i docelowa nie jest katalogiem");
        return -1;
    }
    if(CzyKatalog(sciezkaZrodlowa)==1)
    {
        printf("Scieżka źródłowa nie jest katalogiem\n");
        syslog(LOG_INFO,"Scieżka źródłowa nie jest katalogiem");
        return -1;
    }
    if(CzyKatalog(sciezkaDocelowa)==1)
    {
        printf("Scieżka docelowa nie jest katalogiem\n");
        syslog(LOG_INFO,"Scieżka docelowa nie jest katalogiem");
        return -1;
    }

    double czas=5;
    double rozmiarDzielacyPliki=0;
    bool rekurencja = false;
    int c;
    if(argc>3)
    {
        while ((c = getopt (argc, argv, "t:T:s:S:rR")) != -1)
        {
            switch(c)
            {
                case 't':
                case 'T': //time – czas
                    czas=atof(optarg);
                    printf("%f \n",czas);  
                    break;
                case 's': 
                case 'S': //size – rozmiar
                    rozmiarDzielacyPliki=atof(optarg);
                    printf("%f \n",rozmiarDzielacyPliki);
                    break;
                case 'r':
                case 'R': //rekurencja
                    rekurencja=true;
                    
                    break;
            }
                
        }
        
    }
   
    syslog (LOG_NOTICE, "Demon śpi");
    sleep(czas);
    syslog (LOG_INFO, "Demon został obudzony po %.2f sekundach",czas);

    Pliki *plikiZr;
    Pliki *plikiDoc;
    plikiZr=(Pliki*)calloc(1,sizeof(Pliki)); //pierwszy el zawsze pusty
    plikiDoc=(Pliki*)calloc(1,sizeof(Pliki)); //pierwszy el zawsze pusty


    dodawaniePlikow(sciezkaZrodlowa, plikiZr);
    dodawaniePlikow(sciezkaDocelowa, plikiDoc);

    // printf("-------------------------------------------------\n");
    // wypiszListe(plikiZr);
    // printf("-------------------------------------------------\n");
    // wypiszListe(plikiDoc);

    // signal(SIGUSR1, ignor);//ignoruje demona

    signal(SIGUSR1, handler);


    pid_t p;
    p=fork();
    if(p==0)
    {

        Pliki * plikiZrKopia;
        Pliki * plikiDocKopia;
        plikiZrKopia = plikiZr->nastepny;
        plikiDocKopia = plikiDoc->nastepny;
        int sk=0;



        plikiZr=plikiZrKopia;
        while(plikiZr!=NULL)
        {

            plikiDoc=plikiDocKopia;
            bool czy=czyIstnieje(plikiDoc,plikiZr->nazwaPliku);
            if(czy==false)
            {
                char sc3[50];
                char sc4[50];
                strcpy(sc3,sciezkaZrodlowa);
                strcat(sc3,"/");
                strcat(sc3,plikiZr->nazwaPliku);
                strcpy(sc4,sciezkaDocelowa);
                strcat(sc4,"/");
                strcat(sc4,plikiZr->nazwaPliku);
                if(argc==5)
                {
                    if(rozmiarDzielacyPliki<=plikiZr->rozmiarPliku)
                    {
                        kopiowanie_mmap(sc3,sc4);
                        zmianaDaty(sc3,sc4);
                    }
                    else
                    {
                        kopiowanie(sc3,sc4);
                        zmianaDaty(sc3,sc4);
                    }                        
                }   
                else    
                {
                    kopiowanie(sc3,sc4);
                    zmianaDaty(sc3,sc4);
                }         
                        
            }
            else
            {
                plikiDoc=plikiDocKopia;
                while(plikiDoc!=NULL) 
                {
                    if(strcmp(plikiZr->nazwaPliku,plikiDoc->nazwaPliku)==0)
                    {
                        if(strcmp(plikiZr->dataPliku,plikiDoc->dataPliku)!=0)
                        {
                            char sc1[50];
                            char sc2[50];
                            strcpy(sc1,sciezkaZrodlowa);
                            strcat(sc1,"/");
                            strcat(sc1,plikiZr->nazwaPliku);
                            strcpy(sc2,sciezkaDocelowa);
                            strcat(sc2,"/");
                            strcat(sc2,plikiZr->nazwaPliku);    
                            if(argc==5)
                            {
                                if(rozmiarDzielacyPliki<=plikiZr->rozmiarPliku)
                                {
                                    kopiowanie_mmap(sc1,sc2);
                                }
                                else
                                {
                                    kopiowanie(sc1,sc2);
                                    zmianaDaty(sc1,sc2);
                                }                                       
                            }   
                            else
                            {
                                kopiowanie(sc1,sc2);
                                zmianaDaty(sc1,sc2);
                            }
                            
                        }
                        // continue;                        
                    }
                    plikiDoc=plikiDoc->nastepny;
                    
                }
                
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
                char sc5[50];
                strcpy(sc5,sciezkaDocelowa);
                strcat(sc5,"/");
                strcat(sc5,plikiDoc->nazwaPliku);
                remove(sc5);
                syslog(LOG_INFO,"plik %s został usnięty", plikiDoc->nazwaPliku);
            }
            plikiDoc=plikiDoc->nastepny;
        } 


        rekSynchro(sciezkaZrodlowa, sciezkaDocelowa, rekurencja, rozmiarDzielacyPliki);

        syslog (LOG_NOTICE, "Demon działa – synchronizuje");
        sleep(60);
    }
    
    closelog();
}
