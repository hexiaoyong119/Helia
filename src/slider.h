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

#define SLIDER_TYPE_BOX                     slider_get_type ()

G_DECLARE_FINAL_TYPE ( Slider, slider, HELIA, SLIDER, GtkBox )

Slider * slider_new (void);

GtkScale * slider_get_scale ( Slider * );

void slider_set_signal_id ( Slider *, ulong  );

void slider_clear_all ( Slider * );

void slider_update ( Slider *, double , double  );

void slider_set_data ( Slider *, gint64 , uint8_t , gint64 , uint8_t , gboolean );
