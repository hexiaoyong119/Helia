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

#define SCAN_TYPE_WINDOW                    scan_get_type ()

G_DECLARE_FINAL_TYPE ( Scan, scan, SCAN, WINDOW, GtkWindow )

Scan * scan_new (void);

void helia_dvb_init ( int adapter, int frontend );

void scan_set_run ( GtkTreeView *treeview, GtkWindow *win_base, Scan *scan );

void set_lnb_lhs ( GstElement *element, int type_lnb );

char * scan_get_dvb_info ( int adapter, int frontend );

const char * scan_get_info ( const char *data );

const char * scan_get_info_descr_vis ( const char *data, int num );

