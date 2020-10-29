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

#define CONTROLTV_TYPE_WINDOW control_tv_get_type ()

G_DECLARE_FINAL_TYPE ( ControlTv, control_tv, CONTROLTV, WINDOW, GtkWindow )

ControlTv * control_tv_new (void);

void control_tv_set_run ( gboolean , GstElement *, GtkWindow *, ControlTv * );
