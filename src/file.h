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

void helia_add_dir  ( const char *dir,  Player *player );
void helia_add_uri  ( const char *file, Player *player );
void helia_add_file ( const char *file, Player *player );
void helia_start_file ( GFile **files, int n_files, Player *player );

void helia_open_net ( GtkWindow *win_base, Player *player );

/* Returns a newly-allocated string holding the result. Free with free() */
char * helia_time_to_str ( void );

/* Returns a newly-allocated string holding the result. Free with free() */
char * helia_uri_get_path ( const char *uri );

/* Returns a newly-allocated string holding the result. Free with free() */
char * helia_open_dir ( const char *path, GtkWindow *window );

/* Returns a newly-allocated string holding the result. Free with free() */
char * helia_open_file ( const char *path, GtkWindow *window );

/* Returns a GSList containing the filenames. Free the returned list with g_slist_free(), and the filenames with free(). */
GSList * helia_open_files ( const char *path, GtkWindow *window );

/* Returns a newly-allocated string holding the result. Free with free() */
char * helia_save_file ( const char *dir, const char *file, const char *name_filter, const char *filter_set, GtkWindow *window );

