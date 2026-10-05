#include "vehicle.h"

#include <math.h>

static double clamp01(double x)
{
	if (x < 0.0)
		return 0.0;
	if (x > 1.0)
		return 1.0;
	return x;
}

void vehicle_init(vehicle_t *v)
{
	/* Same seed values as the C++ member initialisers - a cluster that boots showing
	 * 0 km and 0 km on the odometer looks like a bug in a demo video. */
	v->t = 0.0;
	v->speed = 0.0;
	v->rpm = 1100.0;
	v->gear = 0;
	v->fuel = 0.78;
	v->trip = 247.0;
	v->odo = 5891.0;
	v->coolant = 60;
	v->nav_dist = 500;
	v->power = 0.0;
	v->regen = 0.0;
	v->battery = 1.0;
	v->left_ind = v->right_ind = v->high_beam = v->abs = v->oil = false;
}

void vehicle_tick(vehicle_t *v)
{
	v->t += 0.033;

	/* A ride loop rather than a sawtooth: accelerate, cruise, brake, repeat. Gives the
	 * tacho something believable to do and makes the gear indicator change on its own,
	 * which is what sells a cluster demo in the first ten seconds. */
	const double phase = fmod(v->t, 24.0);
	double target;
	if (phase < 9.0)
		target = 12.0 * phase;                       /* accelerate to ~108 */
	else if (phase < 15.0)
		target = 108.0 + 8.0 * sin(phase);
	else
		target = fmax(0.0, 108.0 - 14.0 * (phase - 15.0));

	v->speed += (target - v->speed) * 0.08;              /* smooth, no steps */
	if (v->speed < 0.2)
		v->speed = 0.0;

	v->gear = v->speed < 1.0 ? 0 : (int)fmin(6, (int)(v->speed / 19.0) + 1);

	/* rpm falls on each upshift instead of tracking speed linearly - the detail that
	 * makes a fake tacho look real. */
	const double in_gear = v->gear ? v->speed - (v->gear - 1) * 19.0 : 0.0;
	v->rpm = v->gear ? 2200.0 + in_gear * 480.0
			 : 1100.0 + 200.0 * sin(v->t * 2.0);

	v->trip += v->speed * 0.033 / 3600.0;
	v->odo  += v->speed * 0.033 / 3600.0;
	v->fuel = fmax(0.05, 0.78 - fmod(v->t, 900.0) / 4200.0);
	v->coolant = 60 + (int)(28.0 * fmin(1.0, v->t / 45.0));
	v->nav_dist = 500 - (int)fmod(v->t * 8.0, 480.0);

	/* Drive and regen are derived from ACCELERATION, not speed - so the wing swings
	 * negative the instant the ride loop starts braking. That reversal is the whole
	 * reason an electric cluster uses a symmetric gauge. */
	const double accel = target - v->speed;
	v->power = clamp01(accel / 9.0);
	v->regen = clamp01(-accel / 9.0);
	v->battery = fmax(0.05, 1.0 - fmod(v->t, 1200.0) / 3000.0);

	const double p = fmod(v->t, 24.0);
	v->left_ind  = p > 15.5 && p < 19.0;
	v->right_ind = p > 4.0  && p < 7.0;
	v->high_beam = p > 9.0  && p < 15.0;
	v->abs       = v->speed > 100.0;
	v->oil       = false;
}

int vehicle_range(const vehicle_t *v)
{
	return (int)(v->fuel * 320.0);
}

char vehicle_ride_mode(const vehicle_t *v)
{
	return v->speed > 95.0 ? 'S' : 'E';
}

char vehicle_direction(const vehicle_t *v)
{
	return v->speed < 1.0 ? 'N' : 'F';
}
