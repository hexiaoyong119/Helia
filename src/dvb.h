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

#define DVB_TYPE_BOX                dvb_get_type ()

G_DECLARE_FINAL_TYPE ( Dvb, dvb, DVB, BOX, GtkBox )

Dvb * dvb_new (void);

void dvb_quit ( Dvb * );

void dvb_run_status ( uint16_t , gboolean , Dvb * );

void dvb_start_channel ( const char *, Dvb * );

