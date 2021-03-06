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
#include <gst/gst.h>

#define PLAYER_TYPE_BOX                   player_get_type ()

G_DECLARE_FINAL_TYPE ( Player, player, PLAYER, BOX, GtkBox )

Player * player_new (void);

void player_quit ( Player * );

void player_run_status ( uint16_t , gboolean , Player * );

void player_add_accel ( GtkApplication *, Player * );

void player_treeview_append ( const char *, const char *, Player * );

