
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#define __USE_XOPEN // Per strptime
#include <time.h>

typedef struct {
  double elevation;
  struct tm time;
} gpxPoint;

void createPoint(gpxPoint *p);

int main() {
	struct tm t2 = {0};
	struct tm t1 = {0};

  t1.tm_sec = 30;
  t1.tm_min = 30;

  t2.tm_hour = 170;
  t2.tm_sec = 30;
  t2.tm_min = 30;

  time_t startTime = mktime(&t1);
  time_t endTime = mktime(&t2);

  double timeInSeconds = difftime(endTime, startTime);

  printf("Time1: %d:%d:%d\n", t1.tm_hour, t1.tm_min, t1.tm_sec);
  printf("Time2: %d:%d:%d\n", t2.tm_hour, t2.tm_min, t2.tm_sec);
  printf("Diff (s): %lf\n", timeInSeconds);
  unsigned int hours = (int) (timeInSeconds / 3600);
  int minutes = (int) ((timeInSeconds - (3600 * hours)) / 60); // fmod(timeInSeconds,3600.0);
  int seconds = (int) (timeInSeconds - (3600 * hours) - (60 * minutes));

  struct tm td = {0};
  td.tm_sec = seconds;
  td.tm_min = minutes;
  td.tm_hour = hours;

  printf("Diff: %dh %dm %ds\n", hours, minutes, seconds);
	printf("Diff (tm): %dh %dm %ds\n", td.tm_hour, td.tm_min, td.tm_sec);

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


