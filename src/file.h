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

void helia_add_dir  ( const char *, Player * );
void helia_add_uri  ( const char *, Player * );
void helia_add_file ( const char *, Player * );
void helia_start_file ( GFile **, int , Player * );

void helia_keyb_win ( GtkWindow * );

void helia_open_net ( GtkWindow *, Player * );

/* Returns a newly-allocated string holding the result. Free with free() */
char * helia_time_to_str ( void );

/* Returns a newly-allocated string holding the result. Free with free() */
char * helia_uri_get_path ( const char * );

/* Returns a newly-allocated string holding the result. Free with free() */
char * helia_open_dir ( const char *, GtkWindow * );

/* Returns a newly-allocated string holding the result. Free with free() */
char * helia_open_file ( const char *, GtkWindow * );

/* Returns a GSList containing the filenames. Free the returned list with g_slist_free(), and the filenames with free(). */
GSList * helia_open_files ( const char *, GtkWindow * );

/* Returns a newly-allocated string holding the result. Free with free() */
char * helia_save_file ( const char *, const char *, const char *, const char *, GtkWindow * );

