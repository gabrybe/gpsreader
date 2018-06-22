// GPSReader: legge un file GPX e ne estrae alcune metriche
// MIT License - gabriele.bernuzzi@studenti.unimi.it
//
// - la distanza totale percorsa
// - il tempo impiegato
// - il dislivello in salita e discesa accumulato
// - la velocità media
// - le quote altimetriche massime e minime raggiunte
// - ...
//
// Alcuni punti chiave dello sviluppo:
//
// - lettura del file XML (in formato GPX, che è uno standard utilizzato da quasi tutti i dispositivi GPS in commercio)
// contenente la traccia del percorso: tramite la libreria libxml2, http://www.xmlsoft.org/
//
// - Calcolo distanze tra ogni singolo punto (latitudine/longitudine) della traccia: applicando la formula dell'emisenoverso 
// (https://it.wikipedia.org/wiki/Formula_dell%27emisenoverso)
//
// Uso: gpsreader [file]
// Compilazione:
// gcc gpsreader.c -o gpsreader.out -I/usr/include/libxml2 -lxml2 -lm
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define __USE_XOPEN // Per strptime, altrimenti la definizione non viene trovata
#include <time.h>

// libxml2
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

// namespace che identifica i GPX
#define GPX_NAMESPACE_STR "http://www.topografix.com/GPX/1/1"

// raggio terrestre (in m) come implementato sui GPS
#define EARTH_RADIUS 6.37813 * 1000 * 1000

// fattore di conversione tra gradi e radianti (pi/180)
#define GRAD_TO_RAD (3.1415926536 / 180)

// carattere usato per rappresentare il grafico altimetrico
#define ALTIGRAPH_FILL_CHAR '*'

// tipi custom: Risultati finali
typedef struct {
  char name[50];
  double distance;
  double ascent;
  double descent;
  double avgspeed;
  double totalTime;
  double minElevation;
  double maxElevation;
} metrics;

// Rappresentazione di un punto GPX
typedef struct {
  double lat;
  double lon;
  double elevation;
  struct tm time;
} gpxPoint;

// unità di distanza e di altezza per la stampa del grafico altimetrico
typedef struct {
  double height;
  double distance;
} altigraphUnits;


// global variable per attivare la modalità di debugging
int _DEBUG_ = 0;

// prototipi delle funzioni
int fileExists(const char *filename);
int processFile(const char *filename);

void getResults(gpxPoint *pointSet, int size, metrics *r);
void printResults(const char *filename, const metrics *r);
void printPoint(const gpxPoint *p, int pointNumber);
void printAsciiAltiGraph(const metrics *r, const gpxPoint *pointSet, int numPoints);
void fillAltiGraphMatrix(int rows, int cols, char matrix[rows][cols], const altigraphUnits *units, const double *avgElevation, int minElevation);
void printAltiGraphMatrix(int rows, int cols, const char matrix[rows][cols], const metrics *results, const altigraphUnits *units);
struct tm seconds2tm(const double *timeInSeconds);

gpxPoint getPointData(const xmlDocPtr doc, const xmlNodePtr pointNode);
double getAscent(const gpxPoint *p1, const gpxPoint *p2);
double getDistance(double lat1, double lon1, double lat2, double lon2);
double getAvgSpeed(double *distance, double *timeInSeconds);
void getAvgElevation(const gpxPoint *pointSet, int size, const altigraphUnits *units, double *avgElevation, int avgElevationSize);

void getTrackName(const xmlDocPtr doc, const xmlNodePtr node, char *trackName);
xmlXPathContextPtr createXPathContext(xmlDocPtr doc, xmlNodePtr node);
xmlXPathObjectPtr getTrackSegments(const xmlXPathContextPtr ctx);
xmlXPathObjectPtr getPoints(const xmlXPathContextPtr ctx);

// main
int main(int argc, char *argv[]) {

  const char *filename = argv[1];
  
  // per attivare il debug
  _DEBUG_ = ((argc > 2) ? atoi(argv[2]) : 0);

  printf("C GPS Reader - v1.0\n");

  // validazione argomenti
  if (argc < 2 || !fileExists(filename)) {
    printf("Uso:\n\tgpsreader [file]\n\n\t[file]\n\tpercorso completo del file GPX da esaminare.\n");
    return 1;
  }

  if (_DEBUG_) { printf("\n\t[Debug mode ON]\n"); }
    
  return processFile(filename);
}

// processing del file XML
int processFile(const char *filename) {

  // Inizializzazione parser libxml
  xmlInitParser();

  // parsing file XML
  xmlDocPtr xmlDoc = xmlParseFile(filename);

  // Creazione di un "contesto" nell'ambito del documento XML nel quale sarà valutata una certa espressione XPath
  // Questo specifico contesto racchiude tutto il documento (verrà usato infatti per cercare le tracce)
  xmlXPathContextPtr docContext = xmlXPathNewContext(xmlDoc);

  // namespace per identificare il contenuto come aderente al formato GPX
  xmlXPathRegisterNs(docContext, (xmlChar*)"gpx", (xmlChar*) GPX_NAMESPACE_STR);

  // Ricerca dei segmenti della traccia: sono nel nodo /gpx/trk/trkseg (posso avere piu' trk in un file)
  xmlXPathObjectPtr trackSegments = getTrackSegments(docContext);
  if(trackSegments == NULL || xmlXPathNodeSetIsEmpty(trackSegments->nodesetval)){
    printf("Non ho trovato tracce nel file \"%s\"\n", filename);
    return 1;
  }

  if (_DEBUG_) { printf("Numero segmenti traccia: %d\n",trackSegments->nodesetval->nodeNr); }

  // loop sulle tracce (nodi "trk")
  xmlNodeSetPtr trackNodes = trackSegments->nodesetval;

  for (int n = 0; n < trackNodes->nodeNr; n++) {

    // contenitore risultati in output
    metrics results = {0};
    
    if (_DEBUG_) { printf("Segmento %d\n", n); }

    xmlNodePtr node = trackNodes->nodeTab[n];

    // Se non è un nodo XML (XML_ELEMENT_NODE è un elemento dell'enum xmlElementType), si passa al prossimo
    if(trackNodes->nodeTab[n]->type != XML_ELEMENT_NODE) continue;

    // nell'ambito di questa traccia devo cercare i punti che la compongono; serve quindi creare un altro contesto per valutare 
    // l'espressione XPath di ricerca
    xmlXPathContextPtr trackContext = createXPathContext(xmlDoc, trackNodes->nodeTab[n]);

    // nome della traccia (si trova nel nodo "name")
    getTrackName(xmlDoc, trackNodes->nodeTab[n], results.name);

    // recupero dei punti della traccia
    xmlXPathObjectPtr points = getPoints(trackContext);

    int numPoints = points->nodesetval->nodeNr;

    // Costruisco un array per contenere i dati di tutti i punti, in modo da non doverli più leggere da XML
    gpxPoint *allPoints = malloc(sizeof(gpxPoint) * numPoints);
    
    for (int p = 0; p < numPoints; p++) {
      allPoints[p] = getPointData(xmlDoc, points->nodesetval->nodeTab[p]);      
    }

    getResults(allPoints, numPoints, &results);
    
    // free
    xmlXPathFreeObject(points);
    xmlXPathFreeContext(trackContext);
    
    // stampa dei risultati finali
    printResults(filename, &results);

    // stampa grafico altimetrico
    printAsciiAltiGraph(&results, allPoints, numPoints);
  
    free(allPoints);
  } // for n
  
  // free
  xmlFreeDoc(xmlDoc);
  xmlCleanupParser();

  return 0;
}

// dato un array di punti, calcola le metriche da inserire nei risultati finali
void getResults(gpxPoint *pointSet, int size, metrics *r) {  

  // variabili di comodo
  gpxPoint currPoint = {0};
  gpxPoint prevPoint = {0};

  // loop sull'array dei punti
  for (int p = 0; p < size; p++) {
    
    // struct con i dati del punto corrente
    currPoint = pointSet[p];

    if (_DEBUG_) { printPoint(&currPoint, p); }

    // se siamo al primo elemento, il punto "precedente" è il punto stesso;
    if (p == 0) {
      prevPoint = currPoint;

      // impostazione di minima e massima altezza a partire dal primo punto, così si ha un termine di paragone
      r->minElevation = currPoint.elevation;
      r->maxElevation = currPoint.elevation;
    }

    // ascesa (o discesa)
    double ascent = getAscent(&currPoint, &prevPoint);

    // dislivello positivo (salita) o negativo (discesa)
    (ascent > 0) ? (r->ascent += ascent) : (r->descent += fabs(ascent));
    
    // distanza dal punto precedente
    r->distance += fabs(getDistance(prevPoint.lat, prevPoint.lon, currPoint.lat, currPoint.lon)); 

    // tempo rispetto al punto precedente
    r->totalTime += difftime(mktime(&(currPoint.time)), mktime(&(prevPoint.time)));

    // quota minima/massima
    if (currPoint.elevation < r->minElevation) {
      r->minElevation = currPoint.elevation;
    }

    if (currPoint.elevation > r->maxElevation) {
      r->maxElevation = currPoint.elevation;
    }

    // il punto appena elaborato diventa il "punto precedente"
    prevPoint = currPoint;
  
  } // for p

  // Calcolo della velocità media
  r->avgspeed = getAvgSpeed(&(r->distance), &(r->totalTime));
}

// dato un array di punti traccia, e un'unità di distanza calcola la quota media per ciascuna unità
void getAvgElevation(const gpxPoint *pointSet, int numPoints, const altigraphUnits *units, double *avgElevation, int avgElevationSize) {

  // variabili di comodo
  gpxPoint currPoint = {0};
  gpxPoint prevPoint = {0};

  double distance = 0.0;
  double cd = 0.0;
  double elevation = 0.0;

  // indice per aggiungere elementi ad avgElevation
  int i = 0;

  // contatore punti parziali (viene resettato ogni volta che si raggiunge l'unità di distanza)
  int pc = 0;

  // flag per stabilire se calcolare la quota media
  int calcAvg = 0;

  // loop sull'array dei punti
  for (int p = 0; p < numPoints; p++, pc++) {

    // struct con i dati del punto corrente
    currPoint = pointSet[p];

    // se siamo al primo elemento, il punto "precedente" è il punto stesso;
    if (p == 0) {
      prevPoint = currPoint;
    }

    // distanza dal punto precedente (in km)
    distance += fabs(getDistance(prevPoint.lat, prevPoint.lon, currPoint.lat, currPoint.lon));

    // calcolo della quota media se ho superato l'unità di distanza, oppure sono all'ultimo punto
    calcAvg = (distance < units->distance && (p < numPoints-1)) ? 0 : 1;

    elevation += currPoint.elevation;

    if (calcAvg) {
      
      if (_DEBUG_) {
        printf("Punto: %d, Distanza: %lf, Unita di distanza %d, quota media %lf; ", p, distance, i, elevation/pc);
      }

      avgElevation[i++] = elevation / (double)pc;
      
      // reset contando anche eventuali "sforamenti" rispetto all'unità di distanza
      distance -= units->distance;

      if (_DEBUG_) {
        printf("Scarto accumulato %lf\n\n ", distance);
      }

      // reset contatori
      pc = 0;
      elevation = 0;
    }

    prevPoint = currPoint;

  } // for p
  
  // stampo l'array avgElevation
  if (_DEBUG_) {
    for (int j = 0; j < avgElevationSize; j++) { printf("\navgElevation[%d]: %lf", j, avgElevation[j]); }
  }

}

// Verifica se esiste il file passato in ingresso
// il qualificatore "const" impedisce la modifica della variabile passata per reference nell'argomento della funzione
int fileExists(const char *filename) {
  FILE *fp = fopen(filename, "r");
  if (fp!=NULL) {
    fclose(fp);
  }
  return (fp!=NULL);
}

// funzione di utilità: crea un contesto nell'ambito del documento XML nel quale sarà valutata una certa espressione XPath
// a partire da un certo nodo
xmlXPathContextPtr createXPathContext(xmlDocPtr doc, xmlNodePtr node) {
  xmlXPathContextPtr ctx = xmlXPathNewContext(doc);
  // namespace per identificare il contenuto come GPX
  xmlXPathRegisterNs(ctx, (xmlChar*)"gpx", (xmlChar*) GPX_NAMESPACE_STR);
  ctx->node = node;
  return ctx;
}

// recupero delle tracce/segmenti dal document
xmlXPathObjectPtr getTrackSegments(const xmlXPathContextPtr ctx) {
  return xmlXPathEvalExpression((xmlChar*)"//gpx:trk/gpx:trkseg", ctx);  
}

// recupero dei punti di una traccia
xmlXPathObjectPtr getPoints(const xmlXPathContextPtr ctx) {
  return xmlXPathEvalExpression((xmlChar*)"//gpx:trkpt", ctx);  
}

// restituisce il nome della traccia, dal nodo "name"
void getTrackName(const xmlDocPtr doc, const xmlNodePtr node, char *trackName) {
  xmlXPathContextPtr trackContext = createXPathContext(doc, node);
  xmlXPathObjectPtr name = xmlXPathEvalExpression((xmlChar*)"//gpx:name/text()", trackContext);
  
  strcpy(trackName, name->nodesetval->nodeTab[0]->content);

  xmlXPathFreeObject(name);
  xmlXPathFreeContext(trackContext);
}

// converte i dati di un nodo XML "trkpt" in una struct più facilmente manipolabile
gpxPoint getPointData(const xmlDocPtr doc, const xmlNodePtr pointNode) {
  gpxPoint gp;
  char *end;
  struct tm time;
    
  // quota del punto: nel campo "ele"
  xmlXPathContextPtr eleContext = createXPathContext(doc, pointNode);
  xmlXPathObjectPtr elevation = xmlXPathEvalExpression((xmlChar*)"gpx:ele/text()", eleContext);

  gp.elevation = strtod(elevation->nodesetval->nodeTab[0]->content, &end);

  // free
  if (elevation) xmlXPathFreeObject(elevation);
  xmlXPathFreeContext(eleContext);

  // latitudine/longitudine: negli attributi "lat" e "lon" del trkpt
  if (xmlHasProp(pointNode, (xmlChar*)"lat") && xmlHasProp(pointNode, (xmlChar*)"lon")) {
    xmlChar *lat = xmlGetProp(pointNode, (xmlChar*)"lat");
    gp.lat = strtod((char*)lat, &end);

    xmlChar *lon = xmlGetProp(pointNode, (xmlChar*)"lon");
    gp.lon = strtod((char*)lon, &end);
  }

  // tempo: nel campo "time"
  xmlXPathContextPtr timeContext = createXPathContext(doc, pointNode);
  xmlXPathObjectPtr timeResult = xmlXPathEvalExpression((xmlChar*)"gpx:time/text()", timeContext);

  // inizializza a zero tutta la chiave "time", altrimenti si possono verificare errori di "segmentation fault (core dumped)"
  memset(&(gp.time), 0, sizeof(struct tm));
  strptime((char*)timeResult->nodesetval->nodeTab[0]->content, "%Y-%m-%dT%H:%M:%SZ", &(gp.time));

  // free
  if (timeResult) xmlXPathFreeObject(timeResult);
  xmlXPathFreeContext(timeContext);

  return gp;
}

// dislivello tra due punti
double getAscent(const gpxPoint *p1, const gpxPoint *p2) {
  return (p1->elevation - p2->elevation);
}

// distanza tra due punti, applicando la formula dell'emisenoverso
double getDistance(double lat1, double lon1, double lat2, double lon2) {

  double latRad = (lat2 - lat1) * GRAD_TO_RAD;
  double lonRad = (lon2 - lon1) * GRAD_TO_RAD;

  double lat1Rad = lat1 * GRAD_TO_RAD;
  double lat2Rad = lat2 * GRAD_TO_RAD;

  double a = pow(sin(latRad / 2), 2.0) + pow(sin(lonRad / 2), 2.0) * cos(lat1Rad) * cos(lat2Rad);
  double c = 2 * asin(sqrt(a));

  return EARTH_RADIUS * c;
}

// crea una struct tm a partire da un numero di secondi
struct tm seconds2tm(const double *timeInSeconds) {
  unsigned int hours = (int) (*timeInSeconds / 3600.0);
  int minutes = (int) ((*timeInSeconds - (3600 * hours)) / 60.0);
  int seconds = (int) (*timeInSeconds - (3600 * hours) - (60 * minutes));

  struct tm result = {0};

  result.tm_sec = seconds;
  result.tm_min = minutes;
  result.tm_hour = hours;

  return result;
}

// calcola la velocità media in Km/h data una distanza in Km ed un tempo in secondi
double getAvgSpeed(double *distance, double *timeInSeconds) {
  return ((*distance / *timeInSeconds) * 3.6);
}

// stampa una struct gpxPoint
void printPoint(const gpxPoint *p, int pointNumber) {
  
  char formattedTime[80];
  strftime(formattedTime, sizeof(formattedTime), "%Y-%m-%d %H:%M:%s", &(p->time));
  printf("Punto %d\tquota: %.2lf\t%.2f\t%.2f\t%s\n", pointNumber, p->elevation, p->lat, p->lon, formattedTime);
}

// stampa risultati
void printResults(const char *filename, const metrics *r) {

  // si fa qui solo per esigenze di formattazione (in result infatti ci sono solo dati "grezzi", non formattati)
  struct tm totalTime = seconds2tm(&(r->totalTime));
  
  printf("\nRisultati elaborazione file: <%s>\n\n", filename);

  printf("* Nome traccia: <%s>\n\n", r->name);
  printf("* Distanza (Km):\t\t%8.2lf\n", r->distance / 1000);
  printf("* Tempo impiegato (h:m:s):\t%02d:%02d:%02d\n", totalTime.tm_hour, totalTime.tm_min, totalTime.tm_sec);
  printf("* Velocità media (Km/h):\t%8.2lf\n\n", r->avgspeed);
  printf("* Dislivello in salita (m):\t%8.2lf\n", r->ascent);
  printf("* Dislivello in discesa (m):\t%8.2lf\n\n", r->descent);
  printf("* Quota massima (m):\t\t%8.2lf\n", r->maxElevation);
  printf("* Quota minima (m):\t\t%8.2lf\n", r->minElevation);
  printf("\n");
  
}

// grafico altimetrico ascii usando una matrice con caratteri di riempimento
// l'idea è per ogni "unità di distanza" calcolare l'altezza media, 
// e riempire tanti quadretti in altezza quante sono le "unità di altezza" dell'altezza media
void printAsciiAltiGraph(const metrics *r, const gpxPoint *pointSet, int numPoints) {
  
  // dimensioni della matrice
  int cols = 100;
  int rows = 25;

  // matrice 40r * 80c
  char matrix[rows][cols];

  altigraphUnits units;

  // ogni cella, a quanti m di quota corrisponde? (quota max - min): quota max = 40 : x => delta * 40/quota max
  units.height = (r->maxElevation - r->minElevation) / (double)rows;
  // ogni cella, a quanti m di distanza corrisponde? (distanza totale in m/ scala)
  units.distance = (r->distance) / (double)cols;

  if (_DEBUG_) { printf ("Distance Unit (m): %lf Height unit (m): %lf\n", units.distance, units.height); }

  double avgElevation[cols];
  getAvgElevation(pointSet, numPoints, &units, avgElevation, cols);

  fillAltiGraphMatrix(rows, cols, matrix, &units, avgElevation, r->minElevation);

  printAltiGraphMatrix(rows, cols, matrix, r, &units);
}

// riempie la matrice inserendo un numero appropriato di caratteri di riempimento a seconda della quota media di ogni unità di distanza
void fillAltiGraphMatrix(int rows, int cols, char matrix[rows][cols], const altigraphUnits *units, const double *avgElevation, int minElevation) {
  
  int numFillChars[cols];

  for (int i = 0; i < cols; i++) {
    numFillChars[i] = ceil((avgElevation[i] - minElevation) / units->height);    
  }

  // loop sulle righe (a partire dall'ultima)
  for (int r = 0; r < rows; r++) {
    for (int c = 0; c < cols; c++) {
      matrix[r][c]=' ';
      if (r >= rows - numFillChars[c]) {
        matrix[r][c] = ALTIGRAPH_FILL_CHAR;
      }
    } // for c   
  } // for r

}

// stampa il profilo altimetrico della traccia
void printAltiGraphMatrix(int rows, int cols, const char matrix[rows][cols], const metrics *results, const altigraphUnits *units) {
  printf("\nGrafico altimetrico\n");

  for (int r = 0; r < rows; r++) {

    // per ogni riga si stampa l'altitudine
    printf("\n[%4.0lf]", results->maxElevation - (r * units->height));

    for (int c = 0; c < cols; c++) {
      printf("%c", matrix[r][c]);
    } // for c
  } // for r
  printf("\n");
}