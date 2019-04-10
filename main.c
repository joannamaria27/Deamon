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

bool is_dir(const char* path) {
    struct stat buf;
    stat(path, &buf);
    return S_ISDIR(buf.st_mode);
}
bool is_file(const char* path) {
    struct stat buf;
    stat(path, &buf);
    return S_ISREG(buf.st_mode);
}

typedef struct Lista
{
    char nazwaPliku[100];
    struct nazwaPliku* next;
} Lista;
/*
// dodaje nowy wezel do listy
void dodaj(Lista** lista, Lista* nowa)
{
     nowa->next=NULL;

     if((*lista)==NULL)
     {
      *lista = nowa;
                     }
     else
     {
        Lista* wsk = *lista;
        while(wsk->next != NULL)
        {
        wsk = wsk->next;

        }
        wsk->next = nowa;

        }
     }              

// dodaje plik do listy              
void dodajOsobe(Lista** lista)
{    
    char line[500];
    Lista* nowa = (Lista*)malloc(sizeof(Lista));


    printf("Podaj imie: ");
    scanf("%s", nowa->nazwaPliku);


    dodaj(lista, nowa);     
}
*/
volatile int sygnal = 1;

int main(int argc, char **argv)  
{
    if(argc<=1 ) // dod arg by zmienic
    {
        //printf ("błąd: %s ", strerror (errno)); 
        printf("za mało argumnetów\n ");
	    return 1;
    }
	
    if(argc>5) //jesli jest wiecej niż 3 arg
    {
        printf("za duzo argumnetów\n ");
        return 1;
    }

    float czas;

    if((is_dir(argv[2]) == false) && (is_dir(argv[1]) == false))
    {
        printf("Scieżka zródłowa i docelowa nie jest katalogiem\n");
      //  return -1;
    }
    if(is_dir(argv[1]) == false)
    {
        printf("Scieżka zródłowa nie jest katalogiem\n");
       // return -1;  
    }
    if(is_dir(argv[2]) == false)
    {
        printf("Scieżka docelowa nie jest katalogiem\n");
        // return -1;
    }
   
    printf("Oba żródła są katalogami\n");

    setlogmask (LOG_UPTO (LOG_NOTICE)); //maksymalny, najważniejszy log jaki można wysłać;
    openlog ("demon-projekt", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);    //pierwszy arg nazwa programiu, pózniej co ma się wypisać oprócz samej wiadomosci chyba?

    void handler(int signum)
    {
        printf("signal!\n");
        sygnal=0;
    }
    signal(SIGUSR1, handler);

    if(sygnal==0)
    {
        czas = 0;
        //nie działa log
        printf("Dem0n budzi się\n");
        sleep(czas);
        syslog (LOG_NOTICE, "Demon został obudzony poprzez sygnal\n");
    }
    else
    {
        czas=5;
        if(argc==4)  //działa :D
        {
            czas=atof(argv[3]);
            printf("%f\n ",czas);
        }   
        printf("%f \n",czas);
        sleep(czas);
        syslog (LOG_NOTICE, "Demon został obudzony po %f minutach\n",czas);
        closelog (); 
    }

//  while (1) 
//  { 
 //tu działa :)
        pid_t p;
        p=fork();
    
        if (p < 0) //obsługa błędów pid
        {
            exit(EXIT_FAILURE);
        }
        if(p==0)
        {
            execvp("firefox", argv);
            printf("Dziaaaaaała\n");
            //sleep(10);

    //najpierw przejscie przez pliki i jesli:
//while(plikiZ)
//{
   // char pz=(char)plikiZ;
          //  if(is_file(plikiZ)==true)
          //  {
              //  if(plikiZ==plikiD) //nazwa taka sama???????
             //   {
                    
             //   }
             //   else if (  )//data sie nie zgadza
             //   {

             //   }
               // else
                //{
            //brak pliku w folderze docelowym
            //kopiowanie pliku do katalogu docelowego (open,read)

            //działa ale nie czysci na koniec bufora
                    int plik; 
	                plik = open(argv[1],O_RDONLY);  //zamiast argv - kolejny el listy
	                if(plik<0)
	                {
	                    printf ("błąd: %s ", strerror (errno)); 
		                return 1;
	                }
                    int plikD; 
	                plikD = open(argv[2], O_CREAT | O_RDWR  ,777); //zamiast argv - kolejny el listy
                    //czy potrzeba obsługi błędów??
                    int ileZapis=0;
                    int ileOdcz=0;

                    char buffor[200];
                    do
                    {   
                        memset(buffor,0,sizeof(buffor));
                        ileOdcz=read(plik,buffor,sizeof(buffor));
                        printf("%s", buffor);
                       // ileZapis=0;
                        printf("\n");
                        ileZapis=write(plikD,buffor,sizeof(buffor));
                        printf("%s", buffor);
                    }
                    while(ileOdcz>=197);
                    close(plik);
                    close(plikD);
        
                    //zamiast wyswietlania - kopiowanie

                    //zmienić jeszcze to co sie zapisuje
                    syslog (LOG_NOTICE, "plik został skopiowany\n");
            
            
        }




      // plikiZ=plikiZ->next;
    
    

    closelog ();   //zamkniecie logu

    exit(EXIT_SUCCESS);


}
