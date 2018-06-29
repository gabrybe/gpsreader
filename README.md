## GPSReader

Legge un file GPX, ne estrae alcune metriche e disegna un grafico altimetrico in ASCII

Le metriche estratte sono:

- la distanza totale percorsa
- il tempo impiegato
- il dislivello in salita e discesa accumulato
- la velocità media
- le quote altimetriche massime e minime raggiunte


## Uso

`gpsreader [file] [width] [height] [debug]`
     
     [file] nome del file GPX da elaborare
     [width] larghezza (in caratteri) del grafico altimetrico
     [height] altezza (in caratteri) del grafico altimetrico
     [debug] 0 = debug disattivo; 1 = debug attivo

## Analisi tecnica 

Nel file `analisi-tecnica.md`.

## Quick run 

(utilizzando dati di esempio)

`clear && ./gpsreader.out samples/trailrunning.gpx 60 40`


## Punti chiave dello sviluppo

- lettura del file XML (in formato GPX, che è uno standard utilizzato da quasi tutti i dispositivi GPS in commercio)
contenente la traccia del percorso: tramite la libreria libxml2, http://www.xmlsoft.org/

- Calcolo distanze tra ogni singolo punto (latitudine/longitudine) della traccia: applicando la formula dell'emisenoverso 
(https://it.wikipedia.org/wiki/Formula_dell%27emisenoverso)

- Operazioni su date/ore

- Disegno di un grafico altimetrico (in ASCII) con dimensioni personalizzabili
