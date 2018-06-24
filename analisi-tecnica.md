# GPS Reader - Analisi tecnica

## Abstract

Descrivere le implementazioni e le scelte tecniche adottate nello sviluppo del progetto "GPS Reader".

## Descrizione

Il programma riceve in ingresso il percorso di un file .GPX contenente una traccia GPS registrata da un qualsiasi dispositivo e ne estrae alcune metriche significative, quali ad esempio la distanza totale percorsa, il tempo impiegato, la velocità media, il dislivello in salita e discesa accumulato.

Disegna inoltre un grafico altimetrico della traccia utilizzando i caratteri ASCII.

## Parametri in ingresso

`[file]` nome del file GPX da elaborare  
`[width]` larghezza (in caratteri) del grafico altimetrico  
`[height]` altezza (in caratteri) del grafico altimetrico  
`[debug]` 0 = debug disattivo; 1 = debug attivo  

## Algoritmo

Dopo i necessari controlli di esistenza del file GPX, si inizia l'elaborazione del file (funzione `processFile()`, r. 150) avvalendosi delle funzioni della libreria `libxml2`.  

Si estraggono dai nodi del file le informazioni di ogni singolo punto della traccia (`getPointData()`, r. 393), e si trasferiscono per comodità di elaborazione in un array di struct (`allPoints`, r. 205).

La funzione `getResults()` (r. 208) elabora questo array per fornire le metriche riassuntive della traccia.  
Di seguito alcuni punti significativi dell'elaborazione.  

### `getAscent()`: calcolo del dislivello 
Il dislivello tra due punti consecutivi è calcolato come differenza tra le chiavi `elevation` di ciascuno (r. 434)

### `getDistance()`: calcolo della distanza fra due coordinate
Si applica la formula che calcola la distanza fra due punti disposti su un arco di circonferenza, più precisa rispetto al calcolo lineare (r. 446, 447).

La velocità media è calcolata come la distanza totale diviso il tempo impiegato.
Il dato del tempo è gestito come una struct di tipo tm, sulla quale è possibile operare con le funzioni di aritmetica definite nella libreria `time.h` .

### `printResults()`: stampa dei risultati

La funzione stampa, con alcune formattazioni, i dati presenti nella struct `results`.

### `printAltiGraph()`: stampa del grafico altimetrico

L'idea alla base è quella di utilizzare una matrice n x m, in cui ogni colonna rappresenta una frazione della distanza della traccia, ed ogni riga una frazione della quota massima raggiunta.

In base alla dimensione della matrice (che può essere passata tra i parametri di esecuzione del programma) si determinano i valori di tali frazioni (r. 514, 516), e per ogni intervallo di distanza si calcola la sua quota media (funzione `getAvgElevation()`, r. 521).

Si riempie quindi la matrice inserendo nelle sue righe il numero di caratteri di riempimento che caratterizzano la quota media di ogni unità di distanza (funzione `fillAltiGraphMatrix()`, r. 529).

Il disegno finale del grafico, completo di etichette di quota (asse y) e distanza (asse x) è svolto dalla funzione `printAltiGraphMatrix()` (r. 551).

## Spunti di intervento per le prossime versioni

* Gestione di metriche aggiuntive e device dependent: frequenza cardiaca, potenza espressa, temperatura
* Generalizzazione delle funzioni di generazione e stampa della matrice del grafico altimetrico, per poter essere usate anche per stampare il grafico (ad es.) della frequenza cardiaca, della potenza espressa o della temperatura
* Spostamento delle funzioni di stampa della matrice in una libreria separata
* Validazione più approfondita: gestione dell'assenza di dati validi in un punto
* Disegno del grafico altimetrico tramite libreria grafica e salvataggio dell'immagine in formato PNG










