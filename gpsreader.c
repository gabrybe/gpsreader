//
// GPSReader: legge un file GPX e ne estrae alcune metriche
//
// - la distanza totale percorsa
// - il tempo impiegato
// - il dislivello in salita e discesa accumulato
// - la velocità media e massima
// - l'altitudine massima raggiunta
// - ...
//
// Alcuni punti chiave dello sviluppo:
//
// - lettura del file XML (in formato GPX, che è uno standard utilizzato da quasi tutti i dispositivi GPS in commercio)
// contenente la traccia del percorso: tramite la libreria libxml2, http://www.xmlsoft.org/
//
// - Calcolo distanze tra ogni singolo punto (latitudine/longitudine) della traccia: applicando la formula dell'emisenoverso // (https://it.wikipedia.org/wiki/Formula_dell%27emisenoverso)
//
// Uso: gpsreader [file]

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

// libxml2
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

int fileExists(const char *filename);
int parseGpx(const char *filename);

// main
void main(int argc, char *argv[]) {

  char *fn = argv[1];

  printf("C GPS Reader\n");

  // validazione argomenti
  if (argc < 2 || !fileExists(fn)) {
    printf("Uso:\n\tgpsreader [file]\n\n\t[file]\n\tpercorso completo del file GPX da esaminare.\n");
    exit(-1);
  }

   // Initialize libxml
    xmlInitParser();

  // validazione file XML
}


// Verifica se esiste il file passato in ingresso
// il qualificatore "const" impedisce la modifica della variabile passata per reference nell'argomento della funzione
int fileExists(const char *filename) {
  FILE *fp = fopen (filename, "r");
  if (fp!=NULL) fclose (fp);
  return (fp!=NULL);
}

// Parsing del file GPX
int parseGpx(const char *filename) {

}