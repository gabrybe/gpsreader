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
// - Calcolo distanze tra ogni singolo punto (latitudine/longitudine) della traccia: applicando la formula dell'emisenoverso 
// (https://it.wikipedia.org/wiki/Formula_dell%27emisenoverso)
//
// Uso: gpsreader [file]
// Compilazione:
// gcc gpsreader.c -o gpsreader.out -I/usr/include/libxml2 -lxml2 -lm


#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

#define __USE_XOPEN // Per strptime
#include <time.h>

// libxml2
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

// namespace che identifica i GPX
#define GPX_NAMESPACE_STR "http://www.topografix.com/GPX/1/1"

// raggio terrestre (in Km) come implementato sui GPS
#define EARTH_RADIUS 6378.13

// fattore di conversione tra gradi e radianti (pi/180)
#define GRAD_TO_RAD (3.1415926536 / 180)

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

// tipi custom: Rappresentazione di un punto GPX
typedef struct {
  double lat;
  double lon;
  double elevation;
  struct tm time;
} gpxPoint;

// prototipi delle funzioni
int fileExists(const char *filename);

void printResults(const char *filename, const metrics *r);
void printPoint(const gpxPoint *p, int pointNumber);
struct tm seconds2tm(const double *timeInSeconds);

gpxPoint getPointData(const xmlDocPtr doc, const xmlNodePtr pointNode);
double getAscent(const gpxPoint *p1, const gpxPoint *p2);
double getDistance(double lat1, double lon1, double lat2, double lon2);
double getAvgSpeed(double *distance, double *timeInSeconds);

void getTrackName(const xmlDocPtr doc, const xmlNodePtr node, char *trackName);
xmlXPathContextPtr createXPathContext(xmlDocPtr doc, xmlNodePtr node);
xmlXPathObjectPtr getTracks(const xmlXPathContextPtr ctx);
xmlXPathObjectPtr getPoints(const xmlXPathContextPtr ctx);

// main
int main(int argc, char *argv[]) {

  const char *filename = argv[1];

  printf("C GPS Reader\n");

  // validazione argomenti
  if (argc < 2 || !fileExists(filename)) {
    printf("Uso:\n\tgpsreader [file]\n\n\t[file]\n\tpercorso completo del file GPX da esaminare.\n");
    return 1;
  }
  
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
  xmlXPathObjectPtr tracks = getTracks(docContext);
  if(tracks == NULL || xmlXPathNodeSetIsEmpty(tracks->nodesetval)){
    printf("Non ho trovato tracce nel file \"%s\"\n", argv[1]);
    return 1;
  }

  printf("Numero tracce: %d\n",tracks->nodesetval->nodeNr);

  // loop sulle tracce (nodi "trk")
  xmlNodeSetPtr trackNodes = tracks->nodesetval;

  for (int n = 0; n < trackNodes->nodeNr; n++) {

    // contenitore risultati in output
    metrics result = {0};

    printf("Traccia %d\n", n);
    xmlNodePtr node = trackNodes->nodeTab[n];

    // Se non è un nodo XML (XML_ELEMENT_NODE è un elemento dell'enum xmlElementType), si passa al prossimo
    if(trackNodes->nodeTab[n]->type != XML_ELEMENT_NODE) continue;

    // nell'ambito di questa traccia devo cercare i punti che la compongono; serve quindi creare un altro contesto per valutare 
    // l'espressione XPath di ricerca
    xmlXPathContextPtr trackContext = createXPathContext(xmlDoc, trackNodes->nodeTab[n]);

    getTrackName(xmlDoc, trackNodes->nodeTab[n], result.name);

    // recupero dei punti della traccia
    xmlXPathObjectPtr points = getPoints(trackContext);

    // variabili di comodo
    gpxPoint currPoint = {0};
    gpxPoint prevPoint = {0};

    for (int p = 0; p < points->nodesetval->nodeNr; p++) {
      //printf("\tPunto %d\n", p);

      // aggiorno i risultati calcolando
      // * la distanza del punto dal precedente
      // * il dislivello
      // * la velocità media
      xmlNodePtr pointNode = points->nodesetval->nodeTab[p];
      
      currPoint = getPointData(xmlDoc, pointNode);

      // se siamo al primo elemento, il punto "precedente" è il punto stesso;
      if (p == 0) {
        prevPoint = currPoint;

        // impostazione di minima e massima altezza a partire dal primo punto, così si ha un termine di paragone
        result.minElevation = currPoint.elevation;
        result.maxElevation = currPoint.elevation;
      }

      // ascesa (o discesa)
      double ascent = getAscent(&currPoint, &prevPoint);

      // dislivello positivo (salita) o negativo (discesa)
      (ascent > 0) ? (result.ascent += ascent) : (result.descent += fabs(ascent));

      // printPoint(&currPoint, p);
      
      // distanza dal punto precedente
      result.distance += fabs(getDistance(prevPoint.lat, prevPoint.lon, currPoint.lat, currPoint.lon)); 

      // tempo rispetto al punto precedente
      result.totalTime += difftime(mktime(&(currPoint.time)), mktime(&(prevPoint.time)));

      if (currPoint.elevation < result.minElevation) {
        result.minElevation = currPoint.elevation;
      }

      if (currPoint.elevation > result.maxElevation) {
        result.maxElevation = currPoint.elevation;
      }

      // il punto appena elaborato diventa il "punto precedente"
      prevPoint = currPoint;
    
    } // for

    // Calcolo della velocità media
    result.avgspeed = getAvgSpeed(&(result.distance), &(result.totalTime));

    // stampa dei risultati finali
    printResults(filename, &result);

    // free
    xmlXPathFreeObject(points);
    xmlXPathFreeContext(trackContext);
  }
  
  // free
  xmlFreeDoc(xmlDoc);
  xmlCleanupParser();

  // uscita senza errori
  return 0;
}

// Verifica se esiste il file passato in ingresso
// il qualificatore "const" impedisce la modifica della variabile passata per reference nell'argomento della funzione
int fileExists(const char *filename) {
  FILE *fp = fopen (filename, "r");
  if (fp!=NULL) fclose (fp);
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
xmlXPathObjectPtr getTracks(const xmlXPathContextPtr ctx) {
  return xmlXPathEvalExpression((xmlChar*)"//gpx:trk/gpx:trkseg", ctx);  
}

// recupero dei punti di una traccia
xmlXPathObjectPtr getPoints(const xmlXPathContextPtr ctx) {
  return xmlXPathEvalExpression((xmlChar*)"//gpx:trkpt", ctx);  
}

// restituisce il nome della traccia
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
  return ((*distance * 1000 / *timeInSeconds) * 3.6);
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
  
  printf("Risultati elaborazione file: %s\n\n", filename);

  printf("* Nome traccia: %s\n", r->name);
  printf("* Distanza (Km):\t\t%8.2lf\n", r->distance);
  printf("* Tempo impiegato (h:m:s):\t%02d:%02d:%02d\n", totalTime.tm_hour, totalTime.tm_min, totalTime.tm_sec);
  printf("* Velocità media (Km/h):\t%8.2lf\n", r->avgspeed);
  printf("* Dislivello in salita (m):\t%8.2lf\n", r->ascent);
  printf("* Dislivello in discesa (m):\t%8.2lf\n", r->descent);
  printf("* Quota massima (m):\t\t%8.2lf\n", r->maxElevation);
  printf("* Quota minima (m):\t\t%8.2lf\n", r->minElevation);
  printf("\n");
}