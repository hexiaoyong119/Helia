/*
* Copyright 2020 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-3
* file:///usr/share/common-licenses/GPL-3
* http://www.gnu.org/licenses/gpl-3.0.html
*/

#include "file.h"
#include "default.h"
#include "button.h"
#include "treeview.h"

static GtkEntry *net_entry;

/* Returns a newly-allocated string holding the result. Free with free() */
char * helia_time_to_str ( void )
{
	GDateTime *date = g_date_time_new_now_local ();

	char *str_time = g_date_time_format ( date, "%j-%Y-%T" );

	g_date_time_unref ( date );

	return str_time;
}

/* Returns a newly-allocated string holding the result. Free with free() */
char * helia_uri_get_path ( const char *uri )
{
	GFile *file = g_file_new_for_uri ( uri );

	char *path = g_file_get_path ( file );

	g_object_unref ( file );

	return path;
}

static void helia_open_net_play ( const char *file, Player *player )
{
	if ( file && strlen ( file ) > 0 ) helia_add_uri ( file, player );
}

static void helia_open_net_entry_activate ( GtkEntry *entry, Player *player )
{
	helia_open_net_play ( gtk_entry_get_text ( entry ), player );
}

static void helia_open_net_button_activate ( G_GNUC_UNUSED GtkButton *button, Player *player )
{
	helia_open_net_play ( gtk_entry_get_text ( net_entry ), player );
}

static void helia_open_net_clear ( GtkEntry *entry, G_GNUC_UNUSED GtkEntryIconPosition icon_pos, G_GNUC_UNUSED GdkEvent *event )
{
	gtk_entry_set_text ( GTK_ENTRY ( entry ), "" );
}

void helia_open_net ( GtkWindow *win_base, Player *player )
{
	GtkWindow *window = (GtkWindow *)gtk_window_new ( GTK_WINDOW_TOPLEVEL );
	gtk_window_set_title ( window, "" );
	gtk_window_set_modal ( window, TRUE );
	gtk_window_set_icon_name ( window, DEF_ICON );
	gtk_window_set_transient_for ( window, win_base );
	gtk_window_set_position  ( window, GTK_WIN_POS_CENTER_ON_PARENT );

	GtkBox *m_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_VERTICAL,   0 );

	GtkBox *h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_box_set_spacing ( h_box, 10 );

	const char *icon_net = ( helia_check_icon_theme ( "helia-net" ) ) ? "helia-net" : "applications-internet";

	GtkImage *image = helia_create_image ( icon_net, 24 );
	gtk_widget_set_halign ( GTK_WIDGET ( image ), GTK_ALIGN_START );

	const char *icon = helia_check_icon_theme ( "helia-clear" ) ? "helia-clear" : "edit-clear";

	GtkEntry *entry = (GtkEntry *)gtk_entry_new ();
	gtk_entry_set_icon_from_icon_name ( entry, GTK_ENTRY_ICON_SECONDARY, icon );
	g_signal_connect ( entry, "icon-press", G_CALLBACK ( helia_open_net_clear ), NULL );
	g_signal_connect ( entry, "activate", G_CALLBACK ( helia_open_net_entry_activate ), player );
	g_signal_connect_swapped ( entry, "activate", G_CALLBACK ( gtk_widget_destroy ), window );

	net_entry = entry;
	gtk_widget_set_size_request ( GTK_WIDGET (entry), 400, -1 );

	gtk_box_pack_start ( h_box, GTK_WIDGET ( image ), FALSE, FALSE, 0 );
	gtk_box_pack_start ( h_box, GTK_WIDGET ( entry ), TRUE,  TRUE,  0 );

	GtkButton *button_activate = helia_create_button ( h_box, "helia-ok", "✔", ICON_SIZE );
	g_signal_connect ( button_activate, "clicked", G_CALLBACK ( helia_open_net_button_activate ), player );
	g_signal_connect_swapped ( button_activate, "clicked", G_CALLBACK ( gtk_widget_destroy ), window );

	GtkButton *button_close = helia_create_button ( h_box, "helia-exit", "🞬", ICON_SIZE );
	g_signal_connect_swapped ( button_close, "clicked", G_CALLBACK ( gtk_widget_destroy ), window );

	gtk_box_pack_end ( m_box, GTK_WIDGET ( h_box ), FALSE, FALSE, 5 );

	gtk_container_set_border_width ( GTK_CONTAINER ( m_box ), 10 );
	gtk_container_add   ( GTK_CONTAINER ( window ), GTK_WIDGET ( m_box ) );
	gtk_widget_show_all ( GTK_WIDGET ( window ) );
}

/* Returns a newly-allocated string holding the result. Free with free() */
char * helia_open_file ( const char *path, GtkWindow *window )
{
	GtkFileChooserDialog *dialog = ( GtkFileChooserDialog *)gtk_file_chooser_dialog_new (
					" ",  window, GTK_FILE_CHOOSER_ACTION_OPEN,
					"gtk-cancel", GTK_RESPONSE_CANCEL,
					"gtk-open",   GTK_RESPONSE_ACCEPT,
					NULL );

	gtk_window_set_icon_name ( GTK_WINDOW ( dialog ), "document-open" );

	gtk_file_chooser_set_current_folder  ( GTK_FILE_CHOOSER ( dialog ), path );
	gtk_file_chooser_set_select_multiple ( GTK_FILE_CHOOSER ( dialog ), FALSE );

	char *filename = NULL;

	if ( gtk_dialog_run ( GTK_DIALOG ( dialog ) ) == GTK_RESPONSE_ACCEPT )
		filename = gtk_file_chooser_get_filename ( GTK_FILE_CHOOSER ( dialog ) );

	gtk_widget_destroy ( GTK_WIDGET ( dialog ) );

	return filename;
}

/* Returns a GSList containing the filenames. Free the returned list with g_slist_free(), and the filenames with g_free(). */
GSList * helia_open_files ( const char *path, GtkWindow *window )
{
	GtkFileChooserDialog *dialog = ( GtkFileChooserDialog *)gtk_file_chooser_dialog_new (
					" ",  window, GTK_FILE_CHOOSER_ACTION_OPEN,
					"gtk-cancel", GTK_RESPONSE_CANCEL,
					"gtk-open",   GTK_RESPONSE_ACCEPT,
					NULL );

	gtk_window_set_icon_name ( GTK_WINDOW ( dialog ), "document-open" );

	gtk_file_chooser_set_current_folder  ( GTK_FILE_CHOOSER ( dialog ), path );
	gtk_file_chooser_set_select_multiple ( GTK_FILE_CHOOSER ( dialog ), TRUE );

	GSList *files = NULL;

	if ( gtk_dialog_run ( GTK_DIALOG ( dialog ) ) == GTK_RESPONSE_ACCEPT )
		files = gtk_file_chooser_get_filenames ( GTK_FILE_CHOOSER ( dialog ) );

	gtk_widget_destroy ( GTK_WIDGET ( dialog ) );

	return files;
}

/* Returns a newly-allocated string holding the result. Free with free() */
char * helia_open_dir ( const char *path, GtkWindow *window )
{
	GtkFileChooserDialog *dialog = ( GtkFileChooserDialog *)gtk_file_chooser_dialog_new (
					" ",  window, GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
					"gtk-cancel", GTK_RESPONSE_CANCEL,
					"gtk-apply",  GTK_RESPONSE_ACCEPT,
					NULL );

	gtk_window_set_icon_name ( GTK_WINDOW ( dialog ), "folder-open" );

	gtk_file_chooser_set_current_folder ( GTK_FILE_CHOOSER ( dialog ), path );

	char *dirname = NULL;

	if ( gtk_dialog_run ( GTK_DIALOG ( dialog ) ) == GTK_RESPONSE_ACCEPT )
		dirname = gtk_file_chooser_get_filename ( GTK_FILE_CHOOSER ( dialog ) );

	gtk_widget_destroy ( GTK_WIDGET ( dialog ) );

	return dirname;
}

static void helia_dialod_add_filter ( GtkFileChooserDialog *dialog, const char *name, const char *filter_set )
{
	GtkFileFilter *filter = gtk_file_filter_new ();

	gtk_file_filter_set_name ( filter, name );
	gtk_file_filter_add_pattern ( filter, filter_set );
	gtk_file_chooser_add_filter ( GTK_FILE_CHOOSER ( dialog ), filter );
}

/* Returns a newly-allocated string holding the result. Free with free() */
char * helia_save_file ( const char *dir, const char *file, const char *name_filter, const char *filter_set, GtkWindow *window )
{
	GtkFileChooserDialog *dialog = ( GtkFileChooserDialog *)gtk_file_chooser_dialog_new (
					" ", window,   GTK_FILE_CHOOSER_ACTION_SAVE,
					"gtk-cancel",  GTK_RESPONSE_CANCEL,
					"gtk-save",    GTK_RESPONSE_ACCEPT,
					NULL );

	helia_dialod_add_filter ( dialog, name_filter,  filter_set  );

	gtk_window_set_icon_name ( GTK_WINDOW ( dialog ), "document-save" );

	gtk_file_chooser_set_current_folder ( GTK_FILE_CHOOSER ( dialog ), dir );
	gtk_file_chooser_set_do_overwrite_confirmation ( GTK_FILE_CHOOSER ( dialog ), TRUE );
	gtk_file_chooser_set_current_name   ( GTK_FILE_CHOOSER ( dialog ), file );

	char *filename = NULL;

	if ( gtk_dialog_run ( GTK_DIALOG ( dialog ) ) == GTK_RESPONSE_ACCEPT )
		filename = gtk_file_chooser_get_filename ( GTK_FILE_CHOOSER ( dialog ) );

	gtk_widget_destroy ( GTK_WIDGET ( dialog ) );

	return filename;
}



static void helia_add_m3u ( const char *file, Player *player )
{
	char  *contents = NULL;
	GError *err     = NULL;

	if ( g_file_get_contents ( file, &contents, 0, &err ) )
	{
		char **lines = g_strsplit ( contents, "\n", 0 );

		uint i = 0; for ( i = 0; lines[i] != NULL; i++ )
		//for ( i = 0; lines[i] != NULL && *lines[i]; i++ )
		{
			if ( g_str_has_prefix ( lines[i], "#EXTM3U" ) || g_str_has_prefix ( lines[i], " " ) || strlen ( lines[i] ) < 4 ) continue;

			if ( g_str_has_prefix ( lines[i], "#EXTINF" ) )
			{
				char **lines_info = g_strsplit ( lines[i], ",", 0 );

					if ( g_str_has_prefix ( lines[i+1], "#EXTGRP" ) ) i++;

					player_treeview_append ( g_strstrip ( lines_info[1] ), g_strstrip ( lines[i+1] ), player );

				g_strfreev ( lines_info );
				i++;
			}
			else
			{
				if ( g_str_has_prefix ( lines[i], "#" ) || g_str_has_prefix ( lines[i], " " ) || strlen ( lines[i] ) < 4 ) continue;

				char *name = g_path_get_basename ( lines[i] );

				player_treeview_append ( g_strstrip ( name ), g_strstrip ( lines[i] ), player );

				free ( name );
			}
		}

		g_strfreev ( lines );
		free ( contents );
	}
	else
	{
		g_critical ( "%s:: ERROR: %s ", __func__, err->message );
		g_error_free ( err );
	}
}

void helia_add_dir ( const char *dir_path, Player *player )
{
	GDir *dir = g_dir_open ( dir_path, 0, NULL );

	if ( dir )
	{
		const char *name = NULL;

		while ( ( name = g_dir_read_name ( dir ) ) != NULL )
		{
			char *path_name = g_strconcat ( dir_path, "/", name, NULL );

			if ( g_file_test ( path_name, G_FILE_TEST_IS_DIR ) )
				helia_add_dir ( path_name, player ); // Recursion!

			if ( g_file_test ( path_name, G_FILE_TEST_IS_REGULAR ) )
			{
				if ( g_str_has_suffix ( path_name, "m3u" ) || g_str_has_suffix ( path_name, "M3U" ) )
					helia_add_m3u ( path_name, player );
				else
					player_treeview_append ( name, path_name, player );
			}

			free ( path_name );
		}

		g_dir_close ( dir );
	}
	else
	{
		g_critical ( "%s: opening directory %s failed.", __func__, dir_path );
	}
}

void helia_add_file ( const char *file, Player *player )
{
	if ( g_file_test ( file, G_FILE_TEST_IS_DIR ) )
	{
		helia_add_dir ( file, player);
	}

	if ( g_file_test ( file, G_FILE_TEST_IS_REGULAR ) )
	{
		if ( g_str_has_suffix ( file, "m3u" ) || g_str_has_suffix ( file, "M3U" ) )
		{
			helia_add_m3u ( file, player );
		}
		else
		{
			g_autofree char *name = g_path_get_basename ( file );

			player_treeview_append ( name, file, player );
		}
	}
}

void helia_add_uri ( const char *file, Player *player )
{
	g_autofree char *name = g_path_get_basename ( file );

	player_treeview_append ( name, file, player);
}

void helia_start_file ( GFile **files, int n_files, Player *player )
{
	int c = 0; for ( c = 0; c < n_files; c++ )
	{
		char *file_path = g_file_get_path ( files[c] );

		helia_add_file ( file_path, player );

		free ( file_path );
	}
}

