/*
* Copyright 2020 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-3
* file:///usr/share/common-licenses/GPL-3
* http://www.gnu.org/licenses/gpl-3.0.html
*/

#pragma once

#include <gtk/gtk.h>

#define LEVEL_TYPE_BOX level_get_type ()

G_DECLARE_FINAL_TYPE ( Level, level, LEVEL, BOX, GtkBox )

Level * level_new (void);

void level_set_sgn_snr ( uint8_t sgl, uint8_t snr, gboolean lock, gboolean rec, Level *level );
