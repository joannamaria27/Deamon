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


typedef struct Pliki
{
    char *nazwaPliku;
    //typ, data, rozmiar
    struct Pliki * nastepny;
}Pliki;

void dodawanie(Pliki ** glowa, char nazwa[])
{
    Pliki *p,*e;
    e->nastepny=NULL;
    e->nazwaPliku=nazwa;
    p=*glowa;
    if(p!=NULL)
    {
        while(p->nastepny!=NULL)
            p=p->nastepny;
        p->nastepny=e;        
    }
    else
    {
        *glowa=e;
    }   
}

void kopiowanie (char plikZ, char plikD)
{
    int plikZrodlowy = open(plikZ,O_RDONLY);  //zamiast argv - kolejny el listy
	if(plikZ<0)
	{
	    printf ("błąd: %s ", strerror (errno)); 
	    exit(1);
    }
    int plikDocelowy = open(plikD, O_CREAT | O_RDWR  ,777); //zamiast argv - kolejny el listy
    //czy potrzeba obsługi błędów??
    int ileZapis=0;
    int ileOdcz=0;
    char buffor[200];
    do
    {   
        memset(buffor,0,sizeof(buffor));
        ileOdcz=read(plikZ,buffor,sizeof(buffor));
        printf("%s", buffor);
        printf("\n");
        ileZapis=write(plikD,buffor,sizeof(buffor));
        printf("%s", buffor);
    }
    while(ileOdcz>=197);
    close(plikZ);
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
    rozmiarPliku = sys_lseek(plikZ, 0, SEEK_END);
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

    close(plikZ);
    close(plikD);
}


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
            //execvp("firefox", argv);
            printf("Dziaaaaaała\n");
            sleep(10);

 
            //brak pliku w folderze docelowym
            //kopiowanie pliku do katalogu docelowego (open,read)

            //działa ale nie czysci na koniec bufora
                   // kopiowanie(argv[1],argv[2]);
        
                    //zamiast wyswietlania - kopiowanie

                    //zmienić jeszcze to co sie zapisuje
                    syslog(LOG_NOTICE, "plik został skopiowany\n");
            
            
        }

      // plikiZ=plikiZ->next;

    closelog ();   //zamkniecie logu

    exit(EXIT_SUCCESS);


}
