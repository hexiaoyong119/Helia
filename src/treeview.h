/*
* Copyright 2020 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-3
* file:///usr/share/common-licenses/GPL-3
* http://www.gnu.org/licenses/gpl-3.0.html
*/

#pragma once

#include "dvb.h"
#include "player.h"

#include <gtk/gtk.h>

enum cols_n
{
	COL_NUM,
	COL_FLCH,
	COL_DATA,
	NUM_COLS
};

typedef struct _Column Column;

struct _Column
{
	const char *name;
	const char *type;
	uint8_t num;
};

GtkBox * create_treeview_box ( GtkTreeView *treeview );

GtkTreeView * create_treeview ( uint8_t col_n, Column column_n[] );

void helia_treeview_goup ( GtkTreeView *tree_view );

void helia_treeview_down ( GtkTreeView *tree_view );

void helia_treeview_remv ( GtkTreeView *tree_view );

void helia_treeview_to_file ( const char *file, gboolean mp_tv, GtkTreeView *tree_view );
