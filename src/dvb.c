/*
* Copyright 2020 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-3
* file:///usr/share/common-licenses/GPL-3
* http://www.gnu.org/licenses/gpl-3.0.html
*/

#include "dvb.h"
#include "default.h"
#include "file.h"
#include "info.h"
#include "scan.h"
#include "level.h"
#include "button.h"
#include "treeview.h"
#include "helia-eqa.h"
#include "helia-eqv.h"
#include "control-tv.h"

#include <gdk/gdk.h>
#include <gdk/gdkx.h>

#include <gst/video/videooverlay.h>

struct _Dvb
{
	GtkBox parent_instance;

	GtkBox *win_box;
	GtkBox *playlist;
	GtkTreeView *treeview;
	GtkDrawingArea *video;

	Level *level;

	GstElement *playdvb;
	GstElement *dvbsrc;
	GstElement *demux;
	GstElement *volume;
	GstElement *videoblnc;
	GstElement *equalizer;

	ulong xid;
	uint16_t sid;
	uint16_t opacity;

	char *data;

	gboolean rec_tv;
	gboolean checked_video;
	gboolean debug;
	gboolean run;
	gboolean quit;
};

G_DEFINE_TYPE ( Dvb, dvb, GTK_TYPE_BOX );

typedef void ( *fp ) ( Dvb *dvb );

static void dvb_record ( Dvb *dvb );
static void dvb_run_info ( Dvb *dvb );
static void dvb_stop_set_play ( const char *data, Dvb *dvb );
GstElement * dvb_iterate_element ( GstElement *it_e, const char *name1, const char *name2 );

static void dvb_message_dialog ( const char *f_error, const char *file_or_info, GtkMessageType mesg_type, Dvb *dvb )
{
	GtkWindow *window = GTK_WINDOW ( gtk_widget_get_toplevel ( GTK_WIDGET ( dvb->video ) ) );

	GtkMessageDialog *dialog = ( GtkMessageDialog *)gtk_message_dialog_new (
					window,    GTK_DIALOG_MODAL,
					mesg_type, GTK_BUTTONS_CLOSE,
					"%s\n%s",  f_error, file_or_info );

	gtk_dialog_run     ( GTK_DIALOG ( dialog ) );
	gtk_widget_destroy ( GTK_WIDGET ( dialog ) );
}

static void dvb_set_base ( Dvb *dvb )
{
	gst_element_set_state ( dvb->playdvb, GST_STATE_NULL );

	dvb->volume = NULL;
	level_set_sgn_snr ( 0, 0, FALSE, FALSE, dvb->level );

	g_signal_emit_by_name ( dvb, "power-set", FALSE );

	g_signal_emit_by_name ( dvb, "button-clicked", "base" );
}

static void dvb_set_playlist ( Dvb *dvb )
{
	if ( gtk_widget_get_visible ( GTK_WIDGET ( dvb->playlist ) ) )
		gtk_widget_hide ( GTK_WIDGET ( dvb->playlist ) );
	else
		gtk_widget_show ( GTK_WIDGET ( dvb->playlist ) );
}

static void dvb_set_eqa ( Dvb *dvb )
{
	GtkWindow *window = GTK_WINDOW ( gtk_widget_get_toplevel ( GTK_WIDGET ( dvb->video ) ) );

	if ( GST_ELEMENT_CAST ( dvb->playdvb )->current_state == GST_STATE_PLAYING )
		helia_eqa_win ( dvb->opacity, window, dvb->equalizer );

}

static void dvb_set_eqv ( Dvb *dvb )
{
	GtkWindow *window = GTK_WINDOW ( gtk_widget_get_toplevel ( GTK_WIDGET ( dvb->video ) ) );

	if ( GST_ELEMENT_CAST ( dvb->playdvb )->current_state == GST_STATE_PLAYING )
		helia_eqv_win ( dvb->opacity, window, dvb->videoblnc );

}

static void dvb_set_mute ( Dvb *dvb )
{
	if ( GST_ELEMENT_CAST ( dvb->playdvb )->current_state != GST_STATE_PLAYING ) return;

	gboolean mute = FALSE;
	g_object_get ( dvb->volume, "mute", &mute, NULL );
	g_object_set ( dvb->volume, "mute", !mute, NULL );
}

static void dvb_set_stop ( Dvb *dvb )
{
	gst_element_set_state ( dvb->playdvb, GST_STATE_NULL );

	g_signal_emit_by_name ( dvb, "power-set", FALSE );

	dvb->volume = NULL;
	level_set_sgn_snr ( 0, 0, FALSE, FALSE, dvb->level );

	gtk_widget_queue_draw ( GTK_WIDGET ( dvb->video ) );
}

static void dvb_set_rec ( Dvb *dvb )
{
	if ( GST_ELEMENT_CAST ( dvb->playdvb )->current_state != GST_STATE_PLAYING ) return;

	dvb_record ( dvb );
}

static void dvb_set_info ( Dvb *dvb )
{
	if ( GST_ELEMENT_CAST ( dvb->playdvb )->current_state != GST_STATE_PLAYING ) return;

	dvb_run_info ( dvb );
}

static void dvb_set_scan ( Dvb *dvb )
{
	GtkWindow *win_base = GTK_WINDOW ( gtk_widget_get_toplevel ( GTK_WIDGET ( dvb->video ) ) );

	Scan *scan = scan_new ();
	scan_set_run ( dvb->treeview, win_base, scan );
}

static void dvb_clicked_handler ( G_GNUC_UNUSED ControlTv *ctv, uint8_t num, Dvb *dvb )
{
	fp funcs[] =  { dvb_set_base, dvb_set_playlist, dvb_set_eqa,  dvb_set_eqv,  dvb_set_mute, 
			dvb_set_stop, dvb_set_rec,      dvb_set_scan, dvb_set_info, NULL };

	if ( funcs[num] ) funcs[num] ( dvb );
}



static void dvb_playlist_hide ( G_GNUC_UNUSED GtkButton *button, Dvb *dvb )
{
	gtk_widget_hide ( GTK_WIDGET ( dvb->playlist ) );
}

static void dvb_playlist_save ( G_GNUC_UNUSED GtkButton *button, Dvb *dvb )
{
	GtkTreeModel *model = gtk_tree_view_get_model ( GTK_TREE_VIEW ( dvb->treeview ) );

	int ind = gtk_tree_model_iter_n_children ( model, NULL );

	if ( ind == 0 ) return;

	GtkWindow *window = GTK_WINDOW ( gtk_widget_get_toplevel ( GTK_WIDGET ( dvb->video ) ) );

	g_autofree char *path = helia_save_file ( g_get_home_dir (), "gtv-channel.conf", "conf", "*.conf", window );

	if ( path == NULL ) return;

	helia_treeview_to_file ( path, FALSE, dvb->treeview );
}

static GtkBox * dvb_create_treeview_box ( Dvb *dvb )
{
	GtkBox *v_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_VERTICAL, 0 );
	gtk_box_set_spacing ( v_box, 5 );
	gtk_widget_set_margin_start  ( GTK_WIDGET ( v_box ), 5 );
	gtk_widget_set_margin_end    ( GTK_WIDGET ( v_box ), 5 );
	gtk_widget_set_margin_bottom ( GTK_WIDGET ( v_box ), 5 );

	GtkBox *h_box = create_treeview_box ( dvb->treeview );
	gtk_box_pack_start ( v_box, GTK_WIDGET ( h_box ), TRUE, TRUE, 0 );

	h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_box_set_spacing ( h_box, 5 );

	GtkButton *button = helia_create_button ( h_box, "helia-save", "🖴", ICON_SIZE );
	g_signal_connect ( button, "clicked", G_CALLBACK ( dvb_playlist_save ), dvb );

	button = helia_create_button ( h_box, "helia-exit", "🞬", ICON_SIZE );
	g_signal_connect ( button, "clicked", G_CALLBACK ( dvb_playlist_hide ), dvb );

	gtk_box_pack_start ( v_box, GTK_WIDGET ( h_box ), TRUE, TRUE, 0 );

	return v_box;
}

static void dvb_treeview_row_activated ( GtkTreeView *tree_view, GtkTreePath *path, G_GNUC_UNUSED GtkTreeViewColumn *column, Dvb *dvb )
{
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model ( tree_view );

	if ( gtk_tree_model_get_iter ( model, &iter, path ) )
	{
		g_autofree char *data = NULL;

		gtk_tree_model_get ( model, &iter, COL_DATA, &data, -1 );

		dvb_stop_set_play ( data, dvb );
	}
}

static GtkBox * dvb_create_treeview_scroll ( Dvb *dvb )
{
	GtkBox *v_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_VERTICAL, 0 );
	gtk_box_set_spacing ( v_box, 5 );

	GtkScrolledWindow *scroll = (GtkScrolledWindow *)gtk_scrolled_window_new ( NULL, NULL );
	gtk_scrolled_window_set_policy ( scroll, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
	gtk_widget_set_size_request ( GTK_WIDGET ( scroll ), 220, -1 );

	Column column_n[] =
	{
		{ "Num",      "text", COL_NUM  },
		{ "Channels", "text", COL_FLCH },
		{ "Data",     "text", COL_DATA }
	};

	dvb->treeview = create_treeview ( G_N_ELEMENTS ( column_n ), column_n );
	g_signal_connect ( dvb->treeview, "row-activated", G_CALLBACK ( dvb_treeview_row_activated ), dvb );

	gtk_container_add ( GTK_CONTAINER ( scroll ), GTK_WIDGET ( dvb->treeview ) );

	gtk_box_pack_start ( v_box, GTK_WIDGET ( scroll ), TRUE, TRUE, 0 );
	gtk_box_pack_end   ( v_box, GTK_WIDGET ( dvb_create_treeview_box ( dvb ) ), FALSE, FALSE, 0 );

	dvb->level = level_new ();
	gtk_widget_set_margin_start  ( GTK_WIDGET ( dvb->level ), 5 );
	gtk_widget_set_margin_end    ( GTK_WIDGET ( dvb->level ), 5 );
	gtk_box_pack_end ( v_box, GTK_WIDGET ( dvb->level ), FALSE, FALSE, 0 );

	return v_box;
}



static gboolean dvb_video_fullscreen ( GtkWindow *window )
{
	GdkWindowState state = gdk_window_get_state ( gtk_widget_get_window ( GTK_WIDGET ( window ) ) );

	if ( state & GDK_WINDOW_STATE_FULLSCREEN )
		{ gtk_window_unfullscreen ( window ); return FALSE; }
	else
		{ gtk_window_fullscreen   ( window ); return TRUE;  }

	return TRUE;
}

static gboolean dvb_video_press_event ( GtkDrawingArea *draw, GdkEventButton *event, Dvb *dvb )
{
	GtkWindow *window_base = GTK_WINDOW ( gtk_widget_get_toplevel ( GTK_WIDGET ( draw ) ) );

	if ( event->button == 1 )
	{
		if ( event->type == GDK_2BUTTON_PRESS )
		{
			if ( dvb_video_fullscreen ( GTK_WINDOW ( window_base ) ) )
			{
				gtk_widget_hide ( GTK_WIDGET ( dvb->playlist ) );
			}
		}
	}

	if ( event->button == 2 )
	{
		if ( gtk_widget_get_visible ( GTK_WIDGET ( dvb->playlist ) ) )
		{
			gtk_widget_hide ( GTK_WIDGET ( dvb->playlist ) );
		}
		else
		{
			gtk_widget_show ( GTK_WIDGET ( dvb->playlist ) );
		}
	}

	if ( event->button == 3 )
	{
		gboolean play = FALSE;
		if ( GST_ELEMENT_CAST ( dvb->playdvb )->current_state == GST_STATE_PLAYING ) play = TRUE;

		ControlTv *ctv = control_tv_new ();
		control_tv_set_run ( play, dvb->volume, window_base, ctv );
		g_signal_connect ( ctv, "button-click-num", G_CALLBACK ( dvb_clicked_handler ), dvb );
	}

	return TRUE;
}

static void dvb_video_draw_black ( GtkDrawingArea *widget, cairo_t *cr, const char *name, uint16_t size )
{
	GdkRGBA color; color.red = 0; color.green = 0; color.blue = 0; color.alpha = 1.0;

	int width = gtk_widget_get_allocated_width  ( GTK_WIDGET ( widget ) );
	int heigh = gtk_widget_get_allocated_height ( GTK_WIDGET ( widget ) );

	cairo_rectangle ( cr, 0, 0, width, heigh );
	gdk_cairo_set_source_rgba ( cr, &color );
	cairo_fill (cr);

	GdkPixbuf *pixbuf = gtk_icon_theme_load_icon ( gtk_icon_theme_get_default (), name, size, GTK_ICON_LOOKUP_USE_BUILTIN, NULL );

	if ( pixbuf != NULL )
	{
		cairo_rectangle ( cr, 0, 0, width, heigh );
		gdk_cairo_set_source_pixbuf ( cr, pixbuf, ( width / 2  ) - ( 64  / 2 ), ( heigh / 2 ) - ( 64 / 2 ) );
		cairo_fill (cr);
	}

	if ( pixbuf ) g_object_unref ( pixbuf );
}

static gboolean dvb_video_draw_check ( Dvb *dvb )
{
	if ( GST_ELEMENT_CAST ( dvb->playdvb )->current_state < GST_STATE_PLAYING || !dvb->checked_video ) return TRUE;

	return FALSE;
}

static gboolean dvb_video_draw ( GtkDrawingArea *widget, cairo_t *cr, Dvb *dvb )
{
	if ( dvb_video_draw_check ( dvb ) ) dvb_video_draw_black ( widget, cr, "helia-tv", 96 );

	return FALSE;
}

static void dvb_video_realize ( GtkDrawingArea *draw, Dvb *dvb )
{
	dvb->xid = GDK_WINDOW_XID ( gtk_widget_get_window ( GTK_WIDGET ( draw ) ) );

	if ( dvb->debug ) g_message ( "GDK_WINDOW_XID: %ld ", dvb->xid );
}



static GtkPaned * dvb_create_paned ( GtkBox *playlist, GtkDrawingArea *video )
{
	GtkPaned *paned = (GtkPaned *)gtk_paned_new ( GTK_ORIENTATION_HORIZONTAL );
	gtk_paned_add1 ( paned, GTK_WIDGET ( playlist ) );
	gtk_paned_add2 ( paned, GTK_WIDGET ( video    ) );

	return paned;
}



static void dvb_treeview_append ( const char *name, const char *data, Dvb *dvb )
{
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model ( dvb->treeview );

	int ind = gtk_tree_model_iter_n_children ( model, NULL );

	gtk_list_store_append ( GTK_LIST_STORE ( model ), &iter );
	gtk_list_store_set    ( GTK_LIST_STORE ( model ), &iter,
				COL_NUM,  ind + 1,
				COL_FLCH, name,
				COL_DATA, data,
				-1 );
}

static void dvb_treeview_add_channels ( const char *file, Dvb *dvb )
{
	char  *contents = NULL;
	GError *err     = NULL;

	if ( g_file_get_contents ( file, &contents, 0, &err ) )
	{
		char **lines = g_strsplit ( contents, "\n", 0 );

		uint i = 0; for ( i = 0; lines[i] != NULL; i++ )
		// for ( i = 0; lines[i] != NULL && *lines[i]; i++ )
		{
			if ( g_str_has_prefix ( lines[i], "#" ) || strlen ( lines[i] ) < 2 ) continue;

			char **data = g_strsplit ( lines[i], ":", 0 );

			dvb_treeview_append ( data[0], lines[i], dvb );

			g_strfreev ( data );
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

static void dvb_video_drag_in ( G_GNUC_UNUSED GtkDrawingArea *draw, GdkDragContext *ct, G_GNUC_UNUSED int x, G_GNUC_UNUSED int y, 
	GtkSelectionData *s_data, G_GNUC_UNUSED uint info, guint32 time, Dvb *dvb )
{
	char **uris = gtk_selection_data_get_uris ( s_data );

	uint c = 0; for ( c = 0; uris[c] != NULL; c++ )
	{
		char *path = helia_uri_get_path ( uris[c] );

		if ( path && g_str_has_suffix ( path, "gtv-channel.conf" ) )
			dvb_treeview_add_channels ( path, dvb );

		free ( path );
	}

	g_strfreev ( uris );
	gtk_drag_finish ( ct, TRUE, FALSE, time );
}



typedef struct _DvbSet DvbSet;

struct _DvbSet
{
	GstElement *dvbsrc;
	GstElement *demux;
	GstElement *volume;
	GstElement *videoblnc;
	GstElement *equalizer;
};

static gboolean dvb_pad_check_type ( GstPad *pad, const char *type )
{
	gboolean ret = FALSE;

	GstCaps *caps = gst_pad_get_current_caps ( pad );

	const char *name = gst_structure_get_name ( gst_caps_get_structure ( caps, 0 ) );

	if ( g_str_has_prefix ( name, type ) ) ret = TRUE;

	gst_caps_unref (caps);

	return ret;
}

static void dvb_pad_link ( GstPad *pad, GstElement *element, const char *name )
{
	GstPad *pad_va_sink = gst_element_get_static_pad ( element, "sink" );

	if ( gst_pad_link ( pad, pad_va_sink ) == GST_PAD_LINK_OK )
		gst_object_unref ( pad_va_sink );
	else
		g_debug ( "%s:: linking demux/decode name %s video/audio pad failed ", __func__, name );
}

static void dvb_pad_demux_audio ( G_GNUC_UNUSED GstElement *element, GstPad *pad, GstElement *element_audio )
{
	if ( dvb_pad_check_type ( pad, "audio" ) ) dvb_pad_link ( pad, element_audio, "demux audio" );
}

static void dvb_pad_demux_video ( G_GNUC_UNUSED GstElement *element, GstPad *pad, GstElement *element_video )
{
	if ( dvb_pad_check_type ( pad, "video" ) ) dvb_pad_link ( pad, element_video, "demux video" );
}

static void dvb_pad_decode ( G_GNUC_UNUSED GstElement *element, GstPad *pad, GstElement *element_va )
{
	dvb_pad_link ( pad, element_va, "decode  audio / video" );
}

static DvbSet dvb_create_bin ( GstElement *element, gboolean video_enable )
{
	struct dvb_all_list { const char *name; } dvb_all_list_n[] =
	{
		{ "dvbsrc" }, { "tsdemux"   },
		{ "queue2" }, { "decodebin" }, { "audioconvert" }, { "equalizer-nbands" }, { "volume" }, { "autoaudiosink" },
		{ "queue2" }, { "decodebin" }, { "videoconvert" }, { "videobalance"     }, { "autovideosink" }
	};

	DvbSet dvbset;

	GstElement *elements[ G_N_ELEMENTS ( dvb_all_list_n ) ];

	uint c = 0;
	for ( c = 0; c < G_N_ELEMENTS ( dvb_all_list_n ); c++ )
	{
		if ( !video_enable && c > 7 ) continue;

		elements[c] = gst_element_factory_make ( dvb_all_list_n[c].name, NULL );

		if ( !elements[c] )
			g_critical ( "%s:: element (factory make) - %s not created. \n", __func__, dvb_all_list_n[c].name );

		if ( c == 2 ) gst_element_set_name ( elements[c], "queue-tee-audio" );

		gst_bin_add ( GST_BIN ( element ), elements[c] );

		if (  c == 0 || c == 2 || c == 4 || c == 8 || c == 10 ) continue;

		gst_element_link ( elements[c-1], elements[c] );
	}

	g_signal_connect ( elements[1], "pad-added", G_CALLBACK ( dvb_pad_demux_audio ), elements[2] );
	if ( video_enable ) g_signal_connect ( elements[1], "pad-added", G_CALLBACK ( dvb_pad_demux_video ), elements[8] );

	g_signal_connect ( elements[3], "pad-added", G_CALLBACK ( dvb_pad_decode ), elements[4] );
	if ( video_enable ) g_signal_connect ( elements[9], "pad-added", G_CALLBACK ( dvb_pad_decode ), elements[10] );

	g_object_set ( elements[6], "volume", VOLUME, NULL );

	dvbset.dvbsrc = elements[0];
	dvbset.demux  = elements[1];
	dvbset.volume = elements[6];
	dvbset.equalizer = elements[5];
	dvbset.videoblnc = elements[11];

	return dvbset;
}

static void dvb_remove_bin ( GstElement *pipeline, const char *name )
{
	GstIterator *it = gst_bin_iterate_elements ( GST_BIN ( pipeline ) );
	GValue item = { 0, };
	gboolean done = FALSE;

	while ( !done )
	{
		switch ( gst_iterator_next ( it, &item ) )
		{
			case GST_ITERATOR_OK:
			{
				GstElement *element = GST_ELEMENT ( g_value_get_object (&item) );

				char *object_name = gst_object_get_name ( GST_OBJECT ( element ) );

				if ( name && g_strrstr ( object_name, name ) )
				{
					g_debug ( "%s:: Object Not remove: %s \n", __func__, object_name );
				}
				else
				{
					gst_element_set_state ( element, GST_STATE_NULL );
					gst_bin_remove ( GST_BIN ( pipeline ), element );

					g_debug ( "%s:: Object remove: %s \n", __func__, object_name );
				}

				g_free ( object_name );
				g_value_reset (&item);

				break;
			}

			case GST_ITERATOR_RESYNC:
				gst_iterator_resync (it);
				break;

			case GST_ITERATOR_ERROR:
				done = TRUE;
				break;

			case GST_ITERATOR_DONE:
				done = TRUE;
				break;
		}
	}

	g_value_unset ( &item );
	gst_iterator_free ( it );
}

GstElement * dvb_iterate_element ( GstElement *it_e, const char *name1, const char *name2 )
{
	GstIterator *it = gst_bin_iterate_recurse ( GST_BIN ( it_e ) );

	GValue item = { 0, };
	gboolean done = FALSE;

	GstElement *element_ret = NULL;

	while ( !done )
	{
		switch ( gst_iterator_next ( it, &item ) )
		{
			case GST_ITERATOR_OK:
			{
				GstElement *element = GST_ELEMENT ( g_value_get_object (&item) );

				char *object_name = gst_object_get_name ( GST_OBJECT ( element ) );

				if ( g_strrstr ( object_name, name1 ) )
				{
					if ( name2 && g_strrstr ( object_name, name2 ) )
						element_ret = element;
					else
						element_ret = element;
				}

				g_free ( object_name );
				g_value_reset (&item);

				break;
			}

			case GST_ITERATOR_RESYNC:
				gst_iterator_resync (it);
				break;

			case GST_ITERATOR_ERROR:
				done = TRUE;
				break;

			case GST_ITERATOR_DONE:
				done = TRUE;
				break;
		}
	}

	g_value_unset ( &item );
	gst_iterator_free ( it );

	return element_ret;
}

static GstElementFactory * dvb_find_factory ( GstCaps *caps, guint64 e_num )
{
	GList *list, *list_filter;

	static GMutex mutex;

	g_mutex_lock ( &mutex );
		list = gst_element_factory_list_get_elements ( e_num, GST_RANK_MARGINAL );
		list_filter = gst_element_factory_list_filter ( list, caps, GST_PAD_SINK, gst_caps_is_fixed ( caps ) );
	g_mutex_unlock ( &mutex );

	GstElementFactory *factory = GST_ELEMENT_FACTORY_CAST ( list_filter->data );

	gst_plugin_feature_list_free ( list_filter );
	gst_plugin_feature_list_free ( list );

	return factory;
}

static gboolean dvb_typefind_remove ( const char *name, Dvb *dvb )
{
	GstElement *element = dvb_iterate_element ( dvb->playdvb, name, NULL );

	if ( element )
	{
		gst_element_set_state ( element, GST_STATE_NULL );
		gst_bin_remove ( GST_BIN ( dvb->playdvb ), element );

		return TRUE;
	}

	return FALSE;
}

static void dvb_typefind_parser ( GstElement *typefind, uint probability, GstCaps *caps, Dvb *dvb )
{
	const char *name_caps = gst_structure_get_name ( gst_caps_get_structure ( caps, 0 ) );

	GstElementFactory *factory = dvb_find_factory ( caps, GST_ELEMENT_FACTORY_TYPE_PARSER );

	GstElement *mpegtsmux = dvb_iterate_element ( dvb->playdvb, "mpegtsmux", NULL );

	GstElement *element = gst_element_factory_create ( factory, NULL );

	gboolean remove = FALSE;

	if ( g_str_has_prefix ( name_caps, "audio" ) )
	{
		remove = dvb_typefind_remove ( "parser-audio", dvb );
		gst_element_set_name ( element, "parser-audio" );
	}

	if ( g_str_has_prefix ( name_caps, "video" ) )
	{
		remove = dvb_typefind_remove ( "parser-video", dvb );
		gst_element_set_name ( element, "parser-video" );
	}

	if ( remove == FALSE ) gst_element_unlink ( typefind, mpegtsmux );

	gst_bin_add ( GST_BIN ( dvb->playdvb ), element );

	gst_element_link ( typefind, element );
	gst_element_link ( element, mpegtsmux );

	gst_element_set_state ( element, GST_STATE_PLAYING );

	g_debug ( "%s: probability %d%% | name_caps %s ",__func__, probability, name_caps );
}

static DvbSet dvb_create_rec_bin ( GstElement *element, gboolean video_enable, Dvb *dvb )
{
	struct dvb_all_list { const char *name; } dvb_all_list_n[] =
	{
		{ "tsdemux" },
		{ "tee"     }, { "queue2"     }, { "decodebin" }, { "audioconvert" }, { "equalizer-nbands" }, { "volume" }, { "autoaudiosink" },
		{ "tee"     }, { "queue2"     }, { "decodebin" }, { "videoconvert" }, { "videobalance"     }, { "autovideosink" },
		{ "queue2"  }, { "typefind"   },
		{ "queue2"  }, { "typefind"   },
		{ "mpegtsmux" }, { "filesink" }
	};

	DvbSet dvbset;

	GstElement *elements[ G_N_ELEMENTS ( dvb_all_list_n ) ];

	uint8_t c = 0; for ( c = 0; c < G_N_ELEMENTS ( dvb_all_list_n ); c++ )
	{
		if ( !video_enable && ( c > 7 && c < 14 ) ) continue;
		if ( !video_enable && ( c == 16 || c == 17 ) ) continue;

		elements[c] = gst_element_factory_make ( dvb_all_list_n[c].name, NULL );

		if ( !elements[c] )
			g_critical ( "%s:: element (factory make) - %s not created. \n", __func__, dvb_all_list_n[c].name );

		if ( c == 1 ) gst_element_set_name ( elements[c], "queue-tee-audio" );

		gst_bin_add ( GST_BIN ( element ), elements[c] );

		if ( c > 13 ) continue;

		if (  c == 0 || c == 1 || c == 4 || c == 8 || c == 11 ) continue;

		gst_element_link ( elements[c-1], elements[c] );
	}

	g_signal_connect ( elements[0], "pad-added", G_CALLBACK ( dvb_pad_demux_audio ), elements[1] );
	if ( video_enable ) g_signal_connect ( elements[0], "pad-added", G_CALLBACK ( dvb_pad_demux_video ), elements[8] );

	g_signal_connect ( elements[3], "pad-added", G_CALLBACK ( dvb_pad_decode ), elements[4] );
	if ( video_enable ) g_signal_connect ( elements[10], "pad-added", G_CALLBACK ( dvb_pad_decode ), elements[11] );


	gst_element_link_many ( elements[1], elements[14], elements[15], NULL );
	if ( video_enable ) gst_element_link_many ( elements[8], elements[16], elements[17], NULL );

	gst_element_link ( elements[15], elements[18] );
	if ( video_enable ) gst_element_link ( elements[17], elements[18] );

	gst_element_link ( elements[18], elements[19] );

	g_signal_connect ( elements[15], "have-type", G_CALLBACK ( dvb_typefind_parser ), dvb );
	if ( video_enable ) g_signal_connect ( elements[17], "have-type", G_CALLBACK ( dvb_typefind_parser ), dvb );

	g_autofree char *dt = helia_time_to_str ();
	char **lines = g_strsplit ( dvb->data, ":", 0 );

	char path[PATH_MAX] = {};
	sprintf ( path, "%s/%s-%s.m2ts", g_get_home_dir (), lines[0], dt );

	g_object_set ( elements[19], "location", path, NULL );
	g_strfreev ( lines );

	dvbset.demux  = elements[0];
	dvbset.volume = elements[6];
	dvbset.equalizer = elements[5];
	dvbset.videoblnc = elements[12];

	return dvbset;
}

static GstPadProbeReturn dvb_blockpad_probe ( GstPad * pad, GstPadProbeInfo * info, gpointer data )
{
	Dvb *dvb = (Dvb *) data;

	double value = VOLUME;
	g_object_get ( dvb->volume, "volume", &value, NULL );

	gst_element_set_state ( dvb->playdvb, GST_STATE_PAUSED );
	dvb_remove_bin ( dvb->playdvb, "dvbsrc" );

	DvbSet dvbset;
	dvbset = dvb_create_rec_bin ( dvb->playdvb, dvb->checked_video, dvb );

	dvb->equalizer = dvbset.equalizer;
	dvb->videoblnc = dvbset.videoblnc;
	dvb->volume = dvbset.volume;

	gst_element_link ( dvb->dvbsrc, dvbset.demux );

	g_object_set ( dvb->volume, "volume", value, NULL );
	g_object_set ( dvbset.demux, "program-number", dvb->sid, NULL );

	gst_pad_remove_probe ( pad, GST_PAD_PROBE_INFO_ID (info) );
	gst_element_set_state ( dvb->playdvb, GST_STATE_PLAYING );

	return GST_PAD_PROBE_OK;
}

static void dvb_combo_append_text ( GtkComboBoxText *combo, char *name, uint8_t num )
{
	if ( g_strrstr ( name, "_0_" ) )
	{
		char **data = g_strsplit ( name, "_0_", 0 );

		uint32_t pid = 0;
		sscanf ( data[1], "%4x", &pid );

		char buf[100] = {};
		sprintf ( buf, "%d  ( 0x%.4X )", pid, pid );

		gtk_combo_box_text_append_text ( combo, buf );

		g_strfreev ( data );
	}
	else
	{
		char buf[100] = {};
		sprintf ( buf, "%.4d", num + 1 );

		gtk_combo_box_text_append_text ( combo, buf );
	}
}

static uint8_t dvb_add_audio_track ( GtkComboBoxText *combo, GstElement *element )
{
	GstIterator *it = gst_element_iterate_src_pads ( element );
	GValue item = { 0, };

	uint8_t i = 0, num = 0;
	gboolean done = FALSE;

	while ( !done )
	{
		switch ( gst_iterator_next ( it, &item ) )
		{
			case GST_ITERATOR_OK:
			{
				GstPad *pad_src = GST_PAD ( g_value_get_object (&item) );

				char *name = gst_object_get_name ( GST_OBJECT ( pad_src ) );

				if ( g_str_has_prefix ( name, "audio" ) )
				{
					if ( gst_pad_is_linked ( pad_src ) ) num = i;

					dvb_combo_append_text ( combo, name, i );
					i++;
				}

				free ( name );
				g_value_reset (&item);

				break;
			}

			case GST_ITERATOR_RESYNC:
				gst_iterator_resync (it);
				break;

			case GST_ITERATOR_ERROR:
				done = TRUE;
				break;

			case GST_ITERATOR_DONE:
				done = TRUE;
				break;
		}
	}

	g_value_unset ( &item );
	gst_iterator_free ( it );

	return num;
}

static void dvb_change_audio_track ( GstElement *e_unlink, GstElement *e_link, uint8_t num )
{
	GstIterator *it = gst_element_iterate_src_pads ( e_unlink );
	GValue item = { 0, };

	uint8_t i = 0;
	gboolean done = FALSE;

	GstPad *pad_link = NULL;
	GstPad *pad_sink = gst_element_get_static_pad ( e_link, "sink" );

	while ( !done )
	{
		switch ( gst_iterator_next ( it, &item ) )
		{
			case GST_ITERATOR_OK:
			{
				GstPad *pad_src = GST_PAD ( g_value_get_object (&item) );

				char *name = gst_object_get_name ( GST_OBJECT ( pad_src ) );

				if ( g_str_has_prefix ( name, "audio" ) )
				{
					if ( gst_pad_is_linked ( pad_src ) )
					{
						if ( gst_pad_unlink ( pad_src, pad_sink ) )
							g_debug ( "%s: unlink Ok ", __func__ );
						else
							g_warning ( "%s: unlink Failed ", __func__ );
					}
					else
						if ( i == num ) pad_link = pad_src;

					i++;
				}

				free ( name );
				g_value_reset (&item);

				break;
			}

			case GST_ITERATOR_RESYNC:
				gst_iterator_resync (it);
				break;

			case GST_ITERATOR_ERROR:
				done = TRUE;
				break;

			case GST_ITERATOR_DONE:
				done = TRUE;
				break;
		}
	}

	if ( gst_pad_link ( pad_link, pad_sink ) == GST_PAD_LINK_OK )
		g_debug ( "%s: link Ok ", __func__ );
	else
		g_warning ( "%s: link Failed ", __func__ );

	gst_object_unref ( pad_sink );

	g_value_unset ( &item );
	gst_iterator_free ( it );
}

static void dvb_combo_lang_changed ( GtkComboBox *combo, Dvb *dvb )
{
	int num = gtk_combo_box_get_active ( GTK_COMBO_BOX ( combo ) );

	GstElement *e_link = dvb_iterate_element ( dvb->playdvb, "queue-tee-audio", NULL );

	dvb_change_audio_track ( dvb->demux, e_link, (uint8_t)num );
}

static void dvb_run_info ( Dvb *dvb )
{
	GtkWindow *window = GTK_WINDOW ( gtk_widget_get_toplevel ( GTK_WIDGET ( dvb->video ) ) );

	GtkComboBoxText *combo_lang = helia_info_dvb ( dvb->data, window, dvb->dvbsrc );

	if ( combo_lang )
	{
		uint8_t num = dvb_add_audio_track ( combo_lang, dvb->demux );

		gtk_combo_box_set_active ( GTK_COMBO_BOX ( combo_lang ), num );
		g_signal_connect ( combo_lang, "changed", G_CALLBACK ( dvb_combo_lang_changed ), dvb );
	}
}

static void dvb_set_tuning_timeout ( GstElement *element )
{
	guint64 timeout = 0;
	g_object_get ( element, "tuning-timeout", &timeout, NULL );
	g_object_set ( element, "tuning-timeout", (guint64)timeout / 4, NULL );
}

static void dvb_rinit ( GstElement *element )
{
	int adapter = 0, frontend = 0;
	g_object_get ( element, "adapter",  &adapter,  NULL );
	g_object_get ( element, "frontend", &frontend, NULL );

	helia_dvb_init ( adapter, frontend );
}

static void dvb_data_set ( const char *data, GstElement *element, GstElement *demux, Dvb *dvb )
{
	dvb_set_tuning_timeout ( element );

	char **fields = g_strsplit ( data, ":", 0 );
	uint j = 0, numfields = g_strv_length ( fields );

	for ( j = 1; j < numfields; j++ )
	{
		if ( g_strrstr ( fields[j], "audio-pid" ) || g_strrstr ( fields[j], "video-pid" ) ) continue;

		if ( !g_strrstr ( fields[j], "=" ) ) continue;

		char **splits = g_strsplit ( fields[j], "=", 0 );

		if ( dvb->debug ) g_message ( "%s: gst-param %s | gst-value %s ", __func__, splits[0], splits[1] );

		if ( g_strrstr ( splits[0], "polarity" ) )
		{
			if ( splits[1][0] == 'v' || splits[1][0] == 'V' || splits[1][0] == '0' )
				g_object_set ( element, "polarity", "V", NULL );
			else
				g_object_set ( element, "polarity", "H", NULL );

			g_strfreev (splits);

			continue;
		}

		long dat = atol ( splits[1] );

		if ( g_strrstr ( splits[0], "program-number" ) )
		{
			dvb->sid = (uint16_t)dat;
			g_object_set ( demux, "program-number", dat, NULL );
		}
		else if ( g_strrstr ( splits[0], "symbol-rate" ) )
		{
			g_object_set ( element, "symbol-rate", ( dat > 100000) ? dat / 1000 : dat, NULL );
		}
		else if ( g_strrstr ( splits[0], "lnb-type" ) )
		{
			set_lnb_lhs ( element, (int)dat );
		}
		else
		{
			g_object_set ( element, splits[0], dat, NULL );
		}

		g_strfreev (splits);
	}

	g_strfreev (fields);

	dvb_rinit ( element );
}

static gboolean dvb_checked_video ( const char *data )
{
	gboolean video_enable = TRUE;

	if ( !g_strrstr ( data, "video-pid" ) || g_strrstr ( data, "video-pid=0" ) ) video_enable = FALSE;

	return video_enable;
}

static void dvb_stop_set_play ( const char *data, Dvb *dvb )
{
	free ( dvb->data );
	dvb->data = g_strdup ( data );

	double value = VOLUME;
	if ( dvb->volume ) g_object_get ( dvb->volume, "volume", &value, NULL );

	dvb->rec_tv = FALSE;
	dvb_set_stop ( dvb );

	dvb_remove_bin ( dvb->playdvb, NULL );

	dvb->checked_video = dvb_checked_video ( data );

	DvbSet dvbset;
	dvbset = dvb_create_bin ( dvb->playdvb, dvb->checked_video );

	dvb->dvbsrc = dvbset.dvbsrc;
	dvb->demux = dvbset.demux;
	dvb->equalizer = dvbset.equalizer;
	dvb->videoblnc = dvbset.videoblnc;

	dvb->volume = dvbset.volume;
	dvb_data_set ( data, dvbset.dvbsrc, dvbset.demux, dvb );

	g_object_set ( dvb->volume, "volume", value, NULL );

	gst_element_set_state ( dvb->playdvb, GST_STATE_PLAYING );
}

static gboolean dvb_search_channel ( const char *channel, GtkTreeModel *model, Dvb *dvb )
{
	GtkTreeIter iter;
	gboolean valid, break_f = FALSE;

	for ( valid = gtk_tree_model_get_iter_first ( model, &iter ); valid;
		valid = gtk_tree_model_iter_next ( model, &iter ) )
	{
		char *data = NULL;
		gtk_tree_model_get ( model, &iter, COL_DATA, &data, -1 );

		if ( g_str_has_prefix ( data, channel ) )
		{
			break_f = TRUE;
			dvb_stop_set_play ( data, dvb );
			gtk_tree_selection_select_iter ( gtk_tree_view_get_selection ( dvb->treeview ), &iter );
		}

		free ( data );
		if ( break_f ) break;
	}

	return break_f;
}

void dvb_start_channel ( const char *ch, Dvb *dvb )
{
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model ( dvb->treeview );

	int indx = gtk_tree_model_iter_n_children ( model, NULL );
	if ( indx == 0 ) return;

	if ( ch )
	{
		if ( dvb_search_channel ( ch, model, dvb ) ) return;
	}

	if ( gtk_tree_model_get_iter_first ( model, &iter ) )
	{
		g_autofree char *data = NULL;
		gtk_tree_model_get ( model, &iter, COL_DATA, &data, -1 );

		dvb_stop_set_play ( data, dvb );
		gtk_tree_selection_select_iter ( gtk_tree_view_get_selection ( dvb->treeview ), &iter );
	}
}

static void dvb_record ( Dvb *dvb )
{
	if ( dvb->rec_tv )
	{
		dvb->rec_tv = FALSE;
		dvb_set_stop ( dvb );
	}
	else
	{
		GstPad *blockpad = gst_element_get_static_pad ( dvb->dvbsrc, "src" );

		gst_pad_add_probe ( blockpad, GST_PAD_PROBE_TYPE_BLOCK_DOWNSTREAM, dvb_blockpad_probe, dvb, NULL );

		gst_object_unref ( blockpad );
		dvb->rec_tv = TRUE;
	}
}

static GstBusSyncReply dvb_sync_handler ( G_GNUC_UNUSED GstBus *bus, GstMessage *message, Dvb *dvb )
{
	if ( !gst_is_video_overlay_prepare_window_handle_message ( message ) ) return GST_BUS_PASS;

	if ( dvb->xid != 0 )
	{
		GstVideoOverlay *xoverlay = GST_VIDEO_OVERLAY ( GST_MESSAGE_SRC ( message ) );
		gst_video_overlay_set_window_handle ( xoverlay, dvb->xid );

	} else { g_warning ( "Should have obtained window_handle by now!" ); }

	gst_message_unref ( message );

	return GST_BUS_DROP;
}

static void dvb_msg_cng ( G_GNUC_UNUSED GstBus *bus, G_GNUC_UNUSED GstMessage *msg, Dvb *dvb )
{
	if ( GST_MESSAGE_SRC ( msg ) != GST_OBJECT ( dvb->playdvb ) ) return;

	GstState old_state, new_state;
	gst_message_parse_state_changed ( msg, &old_state, &new_state, NULL );

	switch ( new_state )
	{
		case GST_STATE_NULL:
		case GST_STATE_READY:
			break;

		case GST_STATE_PAUSED:
		{
			g_signal_emit_by_name ( dvb, "power-set", FALSE );
			break;
		}

		case GST_STATE_PLAYING:
		{
			if ( dvb->checked_video ) g_signal_emit_by_name ( dvb, "power-set", TRUE );
			break;
		}

		default:
			break;
	}
}

static void dvb_msg_all ( G_GNUC_UNUSED GstBus *bus, GstMessage *msg, Dvb *dvb )
{
	if ( dvb->quit ) return;

	const GstStructure *structure = gst_message_get_structure ( msg );

	if ( structure )
	{
		int signal = 0, snr = 0;
		gboolean lock = FALSE;

		if (  gst_structure_get_int ( structure, "signal", &signal )  )
		{
			gst_structure_get_int     ( structure, "snr",  &snr  );
			gst_structure_get_boolean ( structure, "lock", &lock );

			uint8_t ret_sgl = (uint8_t)(signal*100/0xffff);
			uint8_t ret_snr = (uint8_t)(snr*100/0xffff);

			level_set_sgn_snr ( ret_sgl, ret_snr, lock, dvb->rec_tv, dvb->level );
		}
	}
}

static void dvb_msg_err ( G_GNUC_UNUSED GstBus *bus, GstMessage *msg, Dvb *dvb )
{
	GError *err = NULL;
	char   *dbg = NULL;

	gst_message_parse_error ( msg, &err, &dbg );

	g_critical ( "%s: %s (%s)", __func__, err->message, (dbg) ? dbg : "no details" );

	dvb_message_dialog ( "", err->message, GTK_MESSAGE_ERROR, dvb );

	g_error_free ( err );
	g_free ( dbg );

	if ( GST_ELEMENT_CAST ( dvb->playdvb )->current_state != GST_STATE_PLAYING )
		dvb_set_stop ( dvb );
}

static GstElement * dvb_create ( Dvb *dvb )
{
	GstElement *dvbplay = gst_pipeline_new ( "pipeline" );

	if ( !dvbplay )
	{
		g_critical ( "%s: dvbplay - not created.", __func__ );
		return NULL;
	}

	GstBus *bus = gst_element_get_bus ( dvbplay );

	gst_bus_add_signal_watch_full ( bus, G_PRIORITY_DEFAULT );
	gst_bus_set_sync_handler ( bus, (GstBusSyncHandler)dvb_sync_handler, dvb, NULL );

	g_signal_connect ( bus, "message",        G_CALLBACK ( dvb_msg_all ), dvb );
	g_signal_connect ( bus, "message::error", G_CALLBACK ( dvb_msg_err ), dvb );
	g_signal_connect ( bus, "message::state-changed", G_CALLBACK ( dvb_msg_cng ), dvb );

	gst_object_unref (bus);

	return dvbplay;
}



static void dvb_init ( Dvb *dvb )
{
	dvb->run  = FALSE;
	dvb->quit = FALSE;
	dvb->data = NULL;
	dvb->volume = NULL;
	dvb->opacity = OPACITY;
	dvb->debug = ( g_getenv ( "DVB_DEBUG" ) ) ? TRUE : FALSE;

	GtkBox *box = GTK_BOX ( dvb );
	gtk_orientable_set_orientation ( GTK_ORIENTABLE ( box ), GTK_ORIENTATION_VERTICAL );
	gtk_box_set_spacing ( box, 3 );

	dvb->playdvb = dvb_create ( dvb );

	dvb->playlist = dvb_create_treeview_scroll ( dvb );

	dvb->video = (GtkDrawingArea *)gtk_drawing_area_new ();
	gtk_widget_set_events ( GTK_WIDGET ( dvb->video ), GDK_BUTTON_PRESS_MASK );

	g_signal_connect ( dvb->video, "draw", G_CALLBACK ( dvb_video_draw ), dvb );
	g_signal_connect ( dvb->video, "realize", G_CALLBACK ( dvb_video_realize ), dvb );
	g_signal_connect ( dvb->video, "button-press-event", G_CALLBACK ( dvb_video_press_event ), dvb );

	gtk_drag_dest_set ( GTK_WIDGET ( dvb->video ), GTK_DEST_DEFAULT_ALL, NULL, 0, GDK_ACTION_COPY );
	gtk_drag_dest_add_uri_targets  ( GTK_WIDGET ( dvb->video ) );
	g_signal_connect  ( dvb->video, "drag-data-received", G_CALLBACK ( dvb_video_drag_in ), dvb );

	GtkPaned *paned = dvb_create_paned ( dvb->playlist, dvb->video );
	gtk_box_pack_start ( box, GTK_WIDGET ( paned ), TRUE, TRUE, 0 );

	char path[PATH_MAX];
	sprintf ( path, "%s/helia/gtv-channel.conf", g_get_user_config_dir () );

	if ( g_file_test ( path, G_FILE_TEST_EXISTS ) ) dvb_treeview_add_channels ( path, dvb );
}

static void dvb_autosave ( Dvb *dvb )
{
	char path[PATH_MAX];
	sprintf ( path, "%s/helia/gtv-channel.conf", g_get_user_config_dir () );

	helia_treeview_to_file ( path, FALSE, dvb->treeview );
}

void dvb_quit ( Dvb *dvb )
{
	dvb_autosave ( dvb );

	dvb->quit = TRUE;

	gst_element_set_state ( dvb->playdvb, GST_STATE_NULL );

	g_object_unref ( dvb->playdvb );
}

void dvb_run_status ( uint16_t opacity, gboolean status, Dvb *dvb )
{
	dvb->run = status;
	dvb->opacity = opacity;
}

static void dvb_finalize ( GObject *object )
{
	G_OBJECT_CLASS (dvb_parent_class)->finalize (object);
}

static void dvb_class_init ( DvbClass *class )
{
	G_OBJECT_CLASS (class)->finalize = dvb_finalize;

	g_signal_new ( "button-clicked", G_TYPE_FROM_CLASS ( class ), G_SIGNAL_RUN_FIRST,
		0, NULL, NULL, g_cclosure_marshal_VOID__STRING, G_TYPE_NONE, 1, G_TYPE_STRING );

	g_signal_new ( "power-set", G_TYPE_FROM_CLASS ( class ), G_SIGNAL_RUN_FIRST,
		0, NULL, NULL, g_cclosure_marshal_VOID__STRING, G_TYPE_NONE, 1, G_TYPE_BOOLEAN );
}

Dvb * dvb_new ( void )
{
	return g_object_new ( DVB_TYPE_BOX, NULL );
}
