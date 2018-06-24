GPSReader: legge un file GPX e ne estrae alcune metriche
MIT License - gabriele.bernuzzi@studenti.unimi.it

- la distanza totale percorsa
- il tempo impiegato
- il dislivello in salita e discesa accumulato
- la velocità media
- le quote altimetriche massime e minime raggiunte
- ...

Punti chiave dello sviluppo:

- lettura del file XML (in formato GPX, che è uno standard utilizzato da quasi tutti i dispositivi GPS in commercio)
contenente la traccia del percorso: tramite la libreria libxml2, http://www.xmlsoft.org/

- Calcolo distanze tra ogni singolo punto (latitudine/longitudine) della traccia: applicando la formula dell'emisenoverso 
(https://it.wikipedia.org/wiki/Formula_dell%27emisenoverso)

Uso: gpsreader [file] [width] [height] [debug]
     
     [file] nome del file GPX da elaborare
     [width] larghezza (in caratteri) del grafico altimetrico
     [height] altezza (in caratteri) del grafico altimetrico
     [debug] 0 = debug disattivo; 1 = debug attivo

Compilazione:
gcc gpsreader.c -o gpsreader.out -I/usr/include/libxml2 -lxml2 -lm

Run di esempio:
clear && ./gpsreader.out samples/trailrunning.gpx 60 40
