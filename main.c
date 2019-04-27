#define _XOPEN_SOURCE 500
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
#include <ftw.h>
#include <time.h>
#include <utime.h>
#include <stdbool.h>
#include <getopt.h>


int typPliku(struct stat filestat)
{
    if(S_ISREG(filestat.st_mode)) return 0;
    else
    if(S_ISDIR(filestat.st_mode)) return 1;
    else return -1;
}

void kopiowanie(char * plikZrodlowy, char * plikDocelowy)
{
    int plikZ = open(plikZrodlowy,O_RDONLY); 
    int plikD = open(plikDocelowy, O_CREAT |O_TRUNC| O_RDWR,0666);
	if(plikZ<0)
	{
	    fprintf (stderr, "error: %s\n", strerror (errno));
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
    size_t rozmiarPliku;

    plikZ = open(sciezkaZ, O_RDONLY);
    if(sciezkaZ<0)
	{
	    fprintf (stderr, "error: %s\n", strerror (errno));
	    exit(1);
   	}
    rozmiarPliku = lseek(plikZ, 0, SEEK_END); 
    zr = mmap(NULL, rozmiarPliku, PROT_READ, MAP_PRIVATE, plikZ, 0);

    plikD = open(sciezkaD, O_RDWR | O_CREAT, 777);
    ftruncate(plikD, rozmiarPliku);
    doc = mmap(NULL, rozmiarPliku, PROT_READ | PROT_WRITE, MAP_SHARED, plikD, 0);

    memcpy(doc, zr, rozmiarPliku);
    munmap(zr, rozmiarPliku);
    munmap(doc, rozmiarPliku);
    syslog(LOG_INFO,"plik %s został skopiowany poprzez mapowanie",sciezkaZ);

    close(plikZ);
    close(plikD);
}

int czyKatalog(const char *path) {
   struct stat statbuf;
   if (stat(path, &statbuf) != 0)
       return 0;
   return S_ISDIR(statbuf.st_mode);
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
    struct utimbuf nowa_data;
    
    stat(plikZ, &filestat);
    nowa_data.modtime = filestat.st_mtime;
    nowa_data.actime = filestat.st_atime;
    
    utime(plikD, &nowa_data);   
    chmod(plikD, filestat.st_mode);
}

void dodawaniePlikow(char * argument, Pliki * pliki)
{
    struct dirent *plik; 
    DIR * sciezka; 

    char sc[50];
    char t[100];
    char * ar1;
    float rozmiar;

    if((sciezka = opendir (argument))!=NULL) 
    {        
        while((plik = readdir (sciezka))!=NULL)  
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
        closedir(sciezka); 
    }
    else
    {
        printf("Bład otwarcia podanej scieżki\n");
        exit(1);
    } 
}

static int rmFiles(const char *pathname, const struct stat *sbuf, int type, struct FTW *ftwb)
{
    if(remove(pathname) < 0)
    {
        perror("ERROR: remove");
        return -1;
    }
    return 0;
}

int rekSynchro(char *sciezkaZ, char *sciezkaD, bool rekurencja, long int rozmiar)
{
    struct dirent *plik;
    struct stat filestatZ;
    struct stat filestatD;
    DIR *plikZ;
    DIR *plikD;
    char scZrodlowa[100];
    char scDocelowa[100];
    
    if (((plikZ = opendir(sciezkaZ)) == NULL) || ((plikD = opendir(sciezkaD)) == NULL))
    {
        syslog(LOG_ERR,"Blad otwarcia katalogu\n");
        return -1;
    }
    
    while ((plik = readdir(plikZ)) != NULL)
    {
        if( (strcmp(plik->d_name,".")==0) || (strcmp(plik->d_name,"..")==0) ) continue;
        filestatD.st_mtime = 0;
        strcpy(scZrodlowa,sciezkaZ);
        strcpy(scDocelowa,sciezkaD);
        strcat(scZrodlowa,"/");
        strcat(scZrodlowa,plik->d_name);
        strcat(scDocelowa,"/");
        strcat(scDocelowa,plik->d_name);
        stat(scZrodlowa,&filestatZ);
        stat(scDocelowa,&filestatD);
        
        switch (typPliku(filestatZ))
        {
            case 0:
                if(filestatZ.st_mtime > filestatD.st_mtime) 
                {
                    if (filestatZ.st_size >= rozmiar) 
                    {
                        kopiowanie_mmap(scZrodlowa,scDocelowa); 
                        zmianaDaty(scZrodlowa,scDocelowa); 
                    }
                    else 
                    {
                        kopiowanie(scZrodlowa,scDocelowa); 
                        zmianaDaty(scZrodlowa,scDocelowa); 
                    }
                }            
                break;
            case 1: 
                if (rekurencja == true) 
                {
                    if (stat(scDocelowa,&filestatD) == -1) 
                    {
                        mkdir(scDocelowa,filestatZ.st_mode); 
                        syslog(LOG_INFO,"stworzono katalog: %s",scDocelowa);
                        rekSynchro(scZrodlowa,scDocelowa,rekurencja,rozmiar); 
                    }
                    else rekSynchro(scZrodlowa,scDocelowa,rekurencja,rozmiar);
                }
                break;
            default: break;
        }
    }
    closedir(plikD);
    closedir(plikZ);
    free(plik);
    return 0;
}

int rekSynchroUsuwanie(char *sciezkaZ, char *sciezkaD, bool rekurencja)
{
    struct dirent *plik;
    struct stat filestatZ;
    struct stat filestatD;
    DIR *plikZ;
    DIR *plikD;
    char scZrodlowa[100];
    char scDocelowa[100];
    
    if (((plikZ = opendir(sciezkaZ)) == NULL) || ((plikD = opendir(sciezkaD)) == NULL))
    {
        syslog(LOG_ERR,"Blad otwarcia katalogu\n");
        return -1;
    }
    
    while ((plik = readdir(plikD)) != NULL)
    {
        if( (strcmp(plik->d_name,".")==0) || (strcmp(plik->d_name,"..")==0) ) continue;
        filestatD.st_mtime = 0;
        strcpy(scZrodlowa,sciezkaZ);
        strcpy(scDocelowa,sciezkaD);
        strcat(scZrodlowa,"/");
        strcat(scZrodlowa,plik->d_name);
        strcat(scDocelowa,"/");
        strcat(scDocelowa,plik->d_name);
        stat(scZrodlowa,&filestatZ);
        stat(scDocelowa,&filestatD);
        

        switch (typPliku(filestatD)) 
        {
            case 0: 
                if(stat(scZrodlowa,&filestatD) == -1)
                {
                    remove(scDocelowa);
                    syslog(LOG_INFO,"plik %s został usuniety",scDocelowa);
                    
                }
                break;
            case 1: 
                if (rekurencja == true)
                {
                    if (stat(scZrodlowa,&filestatD) == -1)
                    {
                           if (nftw(scDocelowa, rmFiles, 10, FTW_DEPTH | FTW_MOUNT | FTW_PHYS) < 0)
                        {
                            printf("nie ma katalogu do usuniecia\n");
                            exit(1);
                        }
                        syslog(LOG_INFO,"katalog %s został usuniety",scDocelowa);
                        rekSynchroUsuwanie(scZrodlowa,scDocelowa,true);
                    }
                    else
                    {
                        rekSynchroUsuwanie(scZrodlowa,scDocelowa,true);
                    }
                    
                }
                break;
            default: break;
        }
    }
    closedir(plikD);
    closedir(plikZ);
    free(plik);
    return 0;
}


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
     if((czyKatalog(sciezkaDocelowa)!=1) && (czyKatalog(sciezkaZrodlowa)!=1))
    {
        printf("Scieżka zródłowa i docelowa nie jest katalogiem\n");
        syslog(LOG_INFO,"Scieżka zródłowa i docelowa nie jest katalogiem");
        return -1;
    }
    if(czyKatalog(sciezkaZrodlowa)!=1)
    {
        printf("Scieżka źródłowa nie jest katalogiem\n");
        syslog(LOG_INFO,"Scieżka źródłowa nie jest katalogiem");
        return -1;
    }
    if(czyKatalog(sciezkaDocelowa)!=1)
    {
        printf("Scieżka docelowa nie jest katalogiem\n");
        syslog(LOG_INFO,"Scieżka docelowa nie jest katalogiem");
        return -1;
    }

    double czas=5*60;
    double rozmiarDzielacyPliki=200;
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
                    break;
                case 's': 
                case 'S': //size – rozmiar
                    rozmiarDzielacyPliki=atof(optarg);
                    break;
                case 'r':
                case 'R': //rekurencja
                    rekurencja=true;                    
                    break;
                default:
                printf("nieprawidłowe argumenty!");
                exit(1);
            }
        }
    }
    
    syslog (LOG_NOTICE, "Demon śpi");
    syslog (LOG_INFO, "Demon zostanie obudzony po %.2f sekundach",czas);
   
    signal(SIGUSR1, handler);
    sleep(czas);

    Pliki *plikiZr;
    Pliki *plikiDoc;

    plikiZr=(Pliki*)calloc(1,sizeof(Pliki)); 
    plikiDoc=(Pliki*)calloc(1,sizeof(Pliki)); 

    dodawaniePlikow(sciezkaZrodlowa, plikiZr);
    dodawaniePlikow(sciezkaDocelowa, plikiDoc);

    // printf("-------------------------------------------------\n");
    // wypiszListe(plikiZr);
    // printf("-------------------------------------------------\n");
    // wypiszListe(plikiDoc);
    syslog (LOG_INFO, "Demon obudził się");
    if(rekurencja==1)
    {
        while (1)
        {
            pid_t pid;
            pid = fork();
            if (pid < 0) {
                exit(EXIT_FAILURE);
            }
            if (pid > 0) {
                exit(EXIT_SUCCESS);
            }

            pid_t r;
            r=fork();
            if(r==0)
            {
                rekSynchro(sciezkaZrodlowa,sciezkaDocelowa,rekurencja,rozmiarDzielacyPliki);
                rekSynchroUsuwanie(sciezkaZrodlowa,sciezkaDocelowa,rekurencja);
            }    
            sleep(czas);    
        }
        
    }
    while(1)
    {
        pid_t pidd;
        pidd = fork();
        if (pidd < 0) {
                exit(EXIT_FAILURE);
        }
        if (pidd > 0) {
                exit(EXIT_SUCCESS);
        }

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
            syslog (LOG_NOTICE, "Demon działa – synchronizuje");
        }
        sleep(czas);
    }
    closelog();
}

