
#include <stdio.h>
#include <stdlib.h>

#define __USE_XOPEN // Per strptime
#include <time.h>
#include <string.h>

typedef struct {
  double elevation;
  struct tm time;
} gpxPoint;

void createPoint(gpxPoint *p);

int main() {
	struct tm t = {0};
	
	printf("Time: %d:%d:%d\n", t.tm_hour, t.tm_min, t.tm_sec);

  gpxPoint gp;

  createPoint(&gp);

	// conversione da struct a stringa
  char buf[80];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S %Z", &(gp.time));
  printf("Converted: %s", buf);
}

void createPoint(gpxPoint *p) {
  p->elevation = 2.0;
  
  memset(&(p->time), 0, sizeof(struct tm));
  strptime((char*)"2018-06-20T15:31:18Z", "%Y-%m-%dT%H:%M:%SZ", &(p->time));
}


