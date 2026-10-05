/*
 * The whole vehicle, simulated - a 1:1 port of MotoCluster's VehicleModel.h.
 *
 * KEEP IT A 1:1 PORT. The only reason this program exists is to answer "what does this
 * board's userspace cost WITHOUT Qt", and that comparison is only honest if both clusters
 * are driven by identical numbers. Every constant below is copied from the C++ original,
 * including the ones that look arbitrary (19.0 km/h per gear, 480 rpm per km/h in gear,
 * the 24 s ride loop). If the simulation is changed, change it in BOTH or the two boards
 * stop being comparable and this whole exercise loses its point.
 *
 * The Qt version is a QObject with a 30 Hz QTimer and Q_PROPERTY bindings. Here it is a
 * plain struct and one tick() - there is no property system to notify, because the LVGL
 * UI reads the struct directly each frame. That difference is the point of the port, not
 * an accident of it.
 */
#ifndef VEHICLE_H
#define VEHICLE_H

#include <stdbool.h>

typedef struct {
	double t;
	double speed;      /* km/h */
	double rpm;
	int    gear;       /* 0 = neutral */
	double fuel;       /* 0..1 */
	double trip;       /* km */
	double odo;        /* km */
	int    coolant;    /* degC */
	int    nav_dist;   /* m to next turn */
	double power;      /* 0..1 drive */
	double regen;      /* 0..1 braking */
	double battery;    /* 0..1 */
	bool   left_ind;
	bool   right_ind;
	bool   high_beam;
	bool   abs;
	bool   oil;
} vehicle_t;

void vehicle_init(vehicle_t *v);

/* One 33 ms step. Same cadence as the original's QTimer(33) - 30 Hz, not 60: the eye
 * cannot tell on a needle sweep and every wakeup is CPU this board would rather not
 * spend. */
void vehicle_tick(vehicle_t *v);

int    vehicle_range(const vehicle_t *v);      /* km, derived from fuel */
char   vehicle_ride_mode(const vehicle_t *v);  /* 'S' above 95 km/h, else 'E' */
char   vehicle_direction(const vehicle_t *v);  /* 'N' below 1 km/h, else 'F' */

#endif /* VEHICLE_H */
