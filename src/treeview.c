/*
* Copyright 2020 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-3
* file:///usr/share/common-licenses/GPL-3
* http://www.gnu.org/licenses/gpl-3.0.html
*/

#include "treeview.h"
#include "default.h"
#include "button.h"

static void helia_treeview_reread ( GtkTreeView *tree_view )
{
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model ( tree_view );

	int row_count = 1;
	gboolean valid;

	for ( valid = gtk_tree_model_get_iter_first ( model, &iter ); valid;
		  valid = gtk_tree_model_iter_next ( model, &iter ) )
	{
		gtk_list_store_set ( GTK_LIST_STORE ( model ), &iter, COL_NUM, row_count++, -1 );
	}
}

static void helia_treeview_up_down ( GtkTreeView *tree_view, gboolean up_dw )
{
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model ( tree_view );

	int ind = gtk_tree_model_iter_n_children ( model, NULL );
	if ( ind < 2 ) return;

	if ( gtk_tree_selection_get_selected ( gtk_tree_view_get_selection ( tree_view ), NULL, &iter ) )
	{
		GtkTreeIter *iter_c = gtk_tree_iter_copy ( &iter );

		if ( up_dw )
		if ( gtk_tree_model_iter_previous ( model, &iter ) )
			gtk_list_store_move_before ( GTK_LIST_STORE ( model ), iter_c, &iter );

		if ( !up_dw )
		if ( gtk_tree_model_iter_next ( model, &iter ) )
			gtk_list_store_move_after ( GTK_LIST_STORE ( model ), iter_c, &iter );

		gtk_tree_iter_free ( iter_c );

		helia_treeview_reread ( tree_view );
	}
}

static void helia_treeview_remove ( GtkTreeView *tree_view )
{
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model ( tree_view );

	if ( gtk_tree_selection_get_selected ( gtk_tree_view_get_selection ( tree_view ), NULL, &iter ) )
	{
		gtk_list_store_remove ( GTK_LIST_STORE ( model ), &iter );

		helia_treeview_reread ( tree_view );
	}
}

void helia_treeview_goup ( GtkTreeView *tree_view )
{
	helia_treeview_up_down ( tree_view, TRUE  );
}

void helia_treeview_down ( GtkTreeView *tree_view )
{
	helia_treeview_up_down ( tree_view, FALSE );
}

void helia_treeview_remv ( GtkTreeView *tree_view )
{
	helia_treeview_remove ( tree_view );
}

void helia_treeview_to_file ( const char *file, gboolean mp_tv, GtkTreeView *tree_view )
{
	GString *gstring = g_string_new ( ( mp_tv ) ? "#EXTM3U \n" : "# Gtv-Dvb channel format \n" );

	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model ( GTK_TREE_VIEW ( tree_view ) );

	int ind = gtk_tree_model_iter_n_children ( model, NULL );

	if ( ind == 0 ) return;

	gboolean valid;
	for ( valid = gtk_tree_model_get_iter_first ( model, &iter ); valid;
		  valid = gtk_tree_model_iter_next ( model, &iter ) )
	{
		char *name = NULL;
		char *data = NULL;

		gtk_tree_model_get ( model, &iter, 1, &name, -1 );
		gtk_tree_model_get ( model, &iter, 2, &data, -1 );

		if ( mp_tv ) g_string_append_printf ( gstring, "#EXTINF:-1,%s\n", name );

		g_string_append_printf ( gstring, "%s\n", data );

		free ( name );
		free ( data );
	}

	GError *err = NULL;

	if ( !g_file_set_contents ( file, gstring->str, -1, &err ) )
	{
		g_critical ( "%s: %s ", __func__, err->message );
		g_error_free ( err );
	}

	g_string_free ( gstring, TRUE );
}

static void playlist_goup ( G_GNUC_UNUSED GtkButton *button, GtkTreeView *treeview )
{
	helia_treeview_goup ( treeview );
}

static void playlist_down ( G_GNUC_UNUSED GtkButton *button, GtkTreeView *treeview )
{
	helia_treeview_down ( treeview );
}

static void playlist_remv ( G_GNUC_UNUSED GtkButton *button, GtkTreeView *treeview )
{
	helia_treeview_remv ( treeview );
}

static void playlist_clr ( G_GNUC_UNUSED GtkButton *button, GtkTreeView *treeview )
{
	gtk_list_store_clear ( GTK_LIST_STORE ( gtk_tree_view_get_model ( treeview ) ) );
}

GtkBox * create_treeview_box ( GtkTreeView *treeview )
{
	GtkBox *h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_box_set_spacing ( h_box, 5 );

	GtkButton *button = helia_create_button ( h_box, "helia-up", "⬆", ICON_SIZE );
	g_signal_connect ( button, "clicked", G_CALLBACK ( playlist_goup ), treeview );

	button = helia_create_button ( h_box, "helia-down", "⬇", ICON_SIZE );
	g_signal_connect ( button, "clicked", G_CALLBACK ( playlist_down ), treeview );

	button = helia_create_button ( h_box, "helia-remove", "➖", ICON_SIZE );
	g_signal_connect ( button, "clicked", G_CALLBACK ( playlist_remv ), treeview );

	button = helia_create_button ( h_box, "helia-clear", "🗑", ICON_SIZE );
	g_signal_connect ( button, "clicked", G_CALLBACK ( playlist_clr  ), treeview );

	return h_box;
}

GtkTreeView * create_treeview ( uint8_t col_n, Column column_n[] )
{
	GtkListStore *store = (GtkListStore *)gtk_list_store_new ( col_n, G_TYPE_UINT, G_TYPE_STRING, G_TYPE_STRING );

	GtkTreeView *treeview = (GtkTreeView *)gtk_tree_view_new_with_model ( GTK_TREE_MODEL ( store ) );

	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	uint8_t c = 0; for ( c = 0; c < col_n; c++ )
	{
		renderer = gtk_cell_renderer_text_new ();

		column = gtk_tree_view_column_new_with_attributes ( column_n[c].name, renderer, column_n[c].type, column_n[c].num, NULL );
		if ( c == COL_DATA ) gtk_tree_view_column_set_visible ( column, FALSE );
		gtk_tree_view_append_column ( treeview, column );
	}

	g_object_unref ( G_OBJECT (store) );

	return treeview;
}

