#GPS Reader - Analisi tecnica

## Abstract

Descrivere le implementazioni e le scelte tecniche adottate nello sviluppo del progetto "GPS Reader".

## Descrizione

Il programma riceve in ingresso il percorso di un file .GPX contenente una traccia GPS registrata da un qualsiasi dispositivo e ne estrae alcune metriche significative, quali ad esempio la distanza totale percorsa, il tempo impiegato, la velocità media, il dislivello in salita e discesa accumulato.

Disegna inoltre un grafico altimetrico della traccia utilizzando i caratteri ASCII.


### Algoritmo

Dopo i necessari controlli di esistenza del file GPX, si inizia l'elaborazione del file (`processFile()`) avvalendosi delle funzioni della libreria `libxml2`.  

Si estraggono dai nodi del file le informazioni di ogni singolo punto della traccia (`getPointData()`), e si trasferiscono per comodità di elaborazione in un array di struct (`allPoints`, r. 205)