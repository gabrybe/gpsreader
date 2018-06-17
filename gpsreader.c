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
#include <math.h>

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
  double distance;
  double ascent;
  double descent;
  double maxspeed;
  double avgspeed;
  double time; // non sarà un double
} metrics;

// tipi custom: Rappresentazione di un punto GPX
typedef struct {
  double lat;
  double lon;
  double elevation;
  double time;
} gpxPoint;


// prototipi delle funzioni
int fileExists(const char *filename);
void printNode(const xmlNodePtr n);
void printResults(const char *filename, const metrics *r);
gpxPoint getPointData(const xmlDocPtr doc, const xmlNodePtr pointNode);
double getAscent(const gpxPoint *p1, const gpxPoint *p2);
double getDistance(double lat1, double lon1, double lat2, double lon2);
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

  // contenitore risultati in output
  metrics result;

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

  // nodi della traccia
  xmlNodeSetPtr trackNodes = tracks->nodesetval;

  for (int n = 0; n < tracks->nodesetval->nodeNr; n++) {
    printf("Traccia %d\n", n);
    xmlNodePtr node = tracks->nodesetval->nodeTab[n];

    // Se non è un nodo XML (XML_ELEMENT_NODE è un elemento dell'enum xmlElementType), si passa al prossimo
    if(trackNodes->nodeTab[n]->type != XML_ELEMENT_NODE) continue;

    // nell'ambito di questa traccia devo cercare i punti che la compongono; serve quindi creare un altro contesto per valutare 
    // l'espressione XPath di ricerca
    xmlXPathContextPtr trackContext = createXPathContext(xmlDoc, trackNodes->nodeTab[n]);
    
    // recupero dei punti della traccia
    xmlXPathObjectPtr points = getPoints(trackContext);

    // variabili di comodo
    gpxPoint currPoint;
    gpxPoint prevPoint;

    for (int p = 0; p < points->nodesetval->nodeNr; p++) {
      //printf("\tPunto %d\n", p);

      // aggiorno i risultati calcolando
      // * la distanza del punto dal precedente
      // * il dislivello
      // * la velocità media e massima
      xmlNodePtr pointNode = points->nodesetval->nodeTab[p];
      
      currPoint = getPointData(xmlDoc, pointNode);

      // se siamo al primo elemento, il punto "precedente" è il punto stesso
      if (p == 0) {
        prevPoint = currPoint;
      }

      // ascesa (o discesa)
      double ascent = getAscent(&currPoint, &prevPoint);
      // dislivello positivo (salita)
      if (ascent > 0) {
        result.ascent += ascent;
      } else {
        // dislivello negativo (discesa)
        result.descent += fabs(ascent);
      }

      printf("Punto %d\tquota: %.2lf\t%.2f\t%.2f\t%.2f\n", p, currPoint.elevation, currPoint.lat, currPoint.lon, ascent);

      // distanza dal punto precedente
      result.distance += fabs(getDistance(prevPoint.lat, prevPoint.lon, currPoint.lat, currPoint.lon)); 

      // aggiornamento delle statistiche
      // updateMetrics(*metrics, currPoint, prevPoint);

      // il punto appena elaborato diventa il "punto precedente"
      prevPoint = currPoint;
    
    } // for

    printNode(node);

    // stampa dei risultati finali
    printResults(filename, &result);

  }

  
  // free
  xmlFreeDoc(xmlDoc);
  xmlCleanupParser();

  // uscita senza errori
  return 0;
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

// Verifica se esiste il file passato in ingresso
// il qualificatore "const" impedisce la modifica della variabile passata per reference nell'argomento della funzione
int fileExists(const char *filename) {
  FILE *fp = fopen (filename, "r");
  if (fp!=NULL) fclose (fp);
  return (fp!=NULL);
}

// recupero delle tracce/segmenti dal document
xmlXPathObjectPtr getTracks(const xmlXPathContextPtr ctx) {
  return xmlXPathEvalExpression((xmlChar*)"//gpx:trk/gpx:trkseg", ctx);  
}

// recupero dei punti di una traccia
xmlXPathObjectPtr getPoints(const xmlXPathContextPtr ctx) {
  return xmlXPathEvalExpression((xmlChar*)"//gpx:trkpt", ctx);  
}

// stampa il contenuto di un nodo XML
void printNode(const xmlNodePtr n) {
  printf("\ttype %d\n", n->type);
  printf("\tname %s\n", n->name);
  printf("\tcontent %s\n", n->content);
}

// stampa risultati
void printResults(const char *filename, const metrics *r) {
  printf("Risultati elaborazione file %s\n\n", filename);
  printf("* Distanza (Km):\t\t%8.2lf\n", r->distance);
  printf("* Dislivello in salita (m):\t%8.2lf\n", r->ascent);
  printf("* Dislivello in discesa (m):\t%8.2lf\n", r->descent);
  printf("* Tempo impiegato:\t%8.2lf\n", r->time);
  printf("* Velocità media (Km/h):\t%8.2lf\n", r->avgspeed);
  printf("* Velocità massima (Km/h):\t%8.2lf\n", r->maxspeed);
  printf("\n");
}

gpxPoint getPointData(const xmlDocPtr doc, const xmlNodePtr pointNode) {
  gpxPoint gp;
  char *end;
  
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