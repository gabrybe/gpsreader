# gpsreader
C Course Project - reads a GPX file and extracts some metrics.

Compile:

gcc /usr/src/gpsreader/gpsreader.c -o /usr/src/gpsreader/gpsreader.out -Iusr/include/libxml2 -lxml2

-I: posizione degli header files di libxml2
-lxml2: specifica di cercare il file libxml2.a nella cartella standard delle librerie

Run:

/usr/src/gpsreader.out

oppure da /usr/src: ./gpsreader.out

