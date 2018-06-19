# gpsreader
C Course Project - reads a GPX file and extracts the following metrics:

* Distance
* Time
* Average speed
* Elevation
* Max/min height

## How to compile:

gcc /usr/src/gpsreader/gpsreader.c -o /usr/src/gpsreader/gpsreader.out -Iusr/include/libxml2 -lxml2 -lm

-I: posizione degli header files di libxml2
-lxml2: specifica di cercare il file libxml2.a nella cartella standard delle librerie
-lm: specifica di caricare anche la libreria libm.a per le funzioni matematiche

Run:

/usr/src/gpsreader.out [nomefile GPX]

oppure da /usr/src: ./gpsreader.out [nomefile GPX]

