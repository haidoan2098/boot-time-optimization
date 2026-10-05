#ifndef CLUSTER_H
#define CLUSTER_H

#include "vehicle.h"

/* Build the whole UI once. Everything that never changes - segment positions, rotations,
 * static labels, the panel chrome - is created here and never touched again.
 *
 * That split is the entire structural argument for this port. In the QML version the
 * scene is instantiated by engine.load() at a measured 1196 ms on this board, because
 * every Rectangle in a Repeater is a QObject with bindings that must be created and
 * evaluated before the first frame. Here the same geometry is a fixed array computed with
 * sinf/cosf in a loop, and the per-frame work drops to writing colours and label text. */
void cluster_build(void);

/* Per-frame update: colours and text only. No object is created, moved or rotated after
 * cluster_build() returns. */
void cluster_update(const vehicle_t *v, double intro, bool live);

#endif /* CLUSTER_H */
