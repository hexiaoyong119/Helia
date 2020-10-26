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

GtkImage * helia_create_image ( const char *icon, uint16_t size );

gboolean helia_check_icon_theme ( const char *name_icon );

GtkButton * helia_create_button ( GtkBox *h_box, const char *name, const char *icon_u, uint16_t icon_size );
