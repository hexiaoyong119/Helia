/*
* Copyright 2020 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-3
* file:///usr/share/common-licenses/GPL-3
* http://www.gnu.org/licenses/gpl-3.0.html
*/

#include "player.h"
#include "default.h"
#include "file.h"
#include "info.h"
#include "slider.h"
#include "button.h"
#include "treeview.h"
#include "helia-eqa.h"
#include "helia-eqv.h"
#include "control-mp.h"
#include "enc-prop.h"
#include "settings.h"

#include <time.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>

#include <gst/video/videooverlay.h>

struct _Player
{
	GtkBox parent_instance;

	GtkBox *win_box;
	GtkBox *playlist;
	GtkLabel *label_buf;
	GtkLabel *label_rec;
	GtkTreeView *treeview;
	GtkDrawingArea *video;

	Slider *slider;

	GstElement *playbin;
	GstElement *volume;
	GstElement *videoblnc;
	GstElement *equalizer;

	GstElement *enc_video;
	GstElement *enc_audio;
	GstElement *enc_muxer;

	GFile *file_rec;
	GstElement *pipeline_rec;

	time_t t_hide;
	time_t t_start;
	gboolean pulse;

	ulong xid;
	uint16_t opacity;

	gboolean run;
	gboolean quit;
	gboolean debug;
	gboolean repeat;
	gboolean rec_video;
};

G_DEFINE_TYPE ( Player, player, GTK_TYPE_BOX );

typedef void ( *fp ) ( Player *player );

static void player_record ( Player *player );
static void player_next_play ( char *file, Player *player );

static void player_message_dialog ( const char *f_error, const char *file_or_info, GtkMessageType mesg_type, Player *player )
{
	GtkWindow *window = GTK_WINDOW ( gtk_widget_get_toplevel ( GTK_WIDGET ( player->video ) ) );

	GtkMessageDialog *dialog = ( GtkMessageDialog *)gtk_message_dialog_new (
					window,    GTK_DIALOG_MODAL,
					mesg_type, GTK_BUTTONS_CLOSE,
					"%s\n%s",  f_error, file_or_info );

	gtk_window_set_icon_name ( GTK_WINDOW (dialog), DEF_ICON );

	gtk_dialog_run     ( GTK_DIALOG ( dialog ) );
	gtk_widget_destroy ( GTK_WIDGET ( dialog ) );
}

static void player_stop_record ( Player *player )
{
	if ( player->pipeline_rec == NULL ) return;

	gst_element_set_state ( player->pipeline_rec, GST_STATE_NULL );

	gst_object_unref ( player->pipeline_rec );

	player->pipeline_rec = NULL;

	slider_clear_all ( player->slider );
	gtk_widget_queue_draw ( GTK_WIDGET ( player->video ) );
}

static void player_set_base ( Player *player )
{
	player_stop_record ( player );

	gst_element_set_state ( player->playbin, GST_STATE_NULL );

	slider_clear_all ( player->slider );

	g_signal_emit_by_name ( player, "power-set", FALSE );

	g_signal_emit_by_name ( player, "button-clicked", "base" );
}

static void player_set_playlist ( Player *player )
{
	if ( gtk_widget_get_visible ( GTK_WIDGET ( player->playlist ) ) )
		gtk_widget_hide ( GTK_WIDGET ( player->playlist ) );
	else
		gtk_widget_show ( GTK_WIDGET ( player->playlist ) );
}

static void player_set_eqa ( Player *player )
{
	GtkWindow *window = GTK_WINDOW ( gtk_widget_get_toplevel ( GTK_WIDGET ( player->video ) ) );

	if ( GST_ELEMENT_CAST ( player->playbin )->current_state == GST_STATE_PLAYING )
		helia_eqa_win ( player->opacity, window, player->equalizer );
}

static void player_set_eqv ( Player *player )
{
	GtkWindow *window = GTK_WINDOW ( gtk_widget_get_toplevel ( GTK_WIDGET ( player->video ) ) );

	if ( GST_ELEMENT_CAST ( player->playbin )->current_state == GST_STATE_PLAYING )
		helia_eqv_win ( player->opacity, window, player->videoblnc );
}

static void player_set_mute ( Player *player )
{
	GstElement *volume = NULL;

	if ( player->pipeline_rec && GST_ELEMENT_CAST ( player->pipeline_rec )->current_state == GST_STATE_PLAYING ) volume = player->volume;

	if ( GST_ELEMENT_CAST ( player->playbin )->current_state == GST_STATE_PLAYING ) volume = player->playbin;

	if ( !volume ) return;

	gboolean mute = FALSE;
	g_object_get ( volume, "mute", &mute, NULL );
	g_object_set ( volume, "mute", !mute, NULL );
}

static void player_set_pause ( Player *player )
{
	g_autofree char *uri = NULL;
	g_object_get ( player->playbin, "current-uri", &uri, NULL );

	if ( uri == NULL ) return;

	if ( GST_ELEMENT_CAST ( player->playbin )->current_state == GST_STATE_PLAYING )
		gst_element_set_state ( player->playbin, GST_STATE_PAUSED  );
	else
		gst_element_set_state ( player->playbin, GST_STATE_PLAYING );
}

static void player_set_stop ( Player *player )
{
	player_stop_record ( player );

	gst_element_set_state ( player->playbin, GST_STATE_NULL );

	slider_clear_all ( player->slider );

	g_signal_emit_by_name ( player, "power-set", FALSE );

	gtk_widget_queue_draw ( GTK_WIDGET ( player->video ) );
}

static void player_set_rec ( Player *player )
{
	player_stop_record ( player );

	if ( GST_ELEMENT_CAST ( player->playbin )->current_state != GST_STATE_PLAYING ) return;

	player_record ( player );
}

static void player_set_info ( Player *player )
{
	if ( GST_ELEMENT_CAST ( player->playbin )->current_state != GST_STATE_PLAYING ) return;

	GtkWindow *window = GTK_WINDOW ( gtk_widget_get_toplevel ( GTK_WIDGET ( player->video ) ) );

	helia_info_player ( window, player->treeview, player->playbin );
}

static void player_stop_set_play ( const char *file, Player *player )
{
	if ( player->pipeline_rec )
	{
		player_message_dialog ( "", "Stop record", GTK_MESSAGE_INFO, player );
		return;
	}

	player_set_stop ( player );

	if ( g_strrstr ( file, "://" ) )
	{
		g_object_set ( player->playbin, "uri", file, NULL );
	}
	else
	{
		g_autofree char *uri = gst_filename_to_uri ( file, NULL );
		g_object_set ( player->playbin, "uri", uri, NULL );
	}

	g_object_set ( player->playbin, "mute", FALSE, NULL );
	gst_element_set_state ( player->playbin, GST_STATE_PLAYING );
}

static void player_clicked_handler ( G_GNUC_UNUSED ControlMp *cmp, uint8_t num, Player *player )
{
	fp funcs[] =  { player_set_base,  player_set_playlist, player_set_eqa, player_set_eqv,  player_set_mute, 
			player_set_pause, player_set_stop,     player_set_rec, player_set_info, NULL };

	if ( funcs[num] ) funcs[num] ( player );
}



static GstBusSyncReply player_sync_handler ( G_GNUC_UNUSED GstBus *bus, GstMessage *message, Player *player )
{
	if ( !gst_is_video_overlay_prepare_window_handle_message ( message ) ) return GST_BUS_PASS;

	if ( player->xid != 0 )
	{
		GstVideoOverlay *xoverlay = GST_VIDEO_OVERLAY ( GST_MESSAGE_SRC ( message ) );
		gst_video_overlay_set_window_handle ( xoverlay, player->xid );

	} else { g_warning ( "Should have obtained window_handle by now!" ); }

	gst_message_unref ( message );

	return GST_BUS_DROP;
}

static void player_msg_cng ( G_GNUC_UNUSED GstBus *bus, G_GNUC_UNUSED GstMessage *msg, Player *player )
{
	if ( GST_MESSAGE_SRC ( msg ) != GST_OBJECT ( player->playbin ) ) return;

	GstState old_state, new_state;

	gst_message_parse_state_changed ( msg, &old_state, &new_state, NULL );

	switch ( new_state )
	{
		case GST_STATE_NULL:
		case GST_STATE_READY:
			break;

		case GST_STATE_PAUSED:
		{
			g_signal_emit_by_name ( player, "power-set", FALSE );
			break;
		}

		case GST_STATE_PLAYING:
		{
			int n_video = 0;
			g_object_get ( player->playbin, "n-video", &n_video, NULL );
			if ( n_video > 0 ) g_signal_emit_by_name ( player, "power-set", TRUE );
			break;
		}

		default:
			break;
	}
}

static void player_msg_buf ( G_GNUC_UNUSED GstBus *bus, GstMessage *msg, Player *player )
{
	if ( player->quit ) return;

	int percent;
	gst_message_parse_buffering ( msg, &percent );

	if ( percent == 100 )
	{
		// if ( GST_ELEMENT_CAST ( player->playbin )->current_state == GST_STATE_PAUSED )
			gst_element_set_state ( player->playbin, GST_STATE_PLAYING );

		gtk_label_set_text ( player->label_buf, " ⇄ 0% " );
	}
	else
	{
		if ( GST_ELEMENT_CAST ( player->playbin )->current_state == GST_STATE_PLAYING )
			gst_element_set_state ( player->playbin, GST_STATE_PAUSED );

		char buf[20] = {};
		sprintf ( buf, " ⇄ %d%% ", percent );

		gtk_label_set_text ( player->label_buf, buf );
	}
}

static void player_msg_eos ( G_GNUC_UNUSED GstBus *bus, G_GNUC_UNUSED GstMessage *msg, Player *player )
{
	g_autofree char *uri = NULL;
	g_object_get ( player->playbin, "current-uri", &uri, NULL );

	if ( uri && ( g_str_has_suffix ( uri, ".png" ) || g_str_has_suffix ( uri, ".jpg" ) ) ) return;

	player_set_stop ( player );

	if ( uri ) player_next_play ( uri, player );
}

static void player_msg_err ( G_GNUC_UNUSED GstBus *bus, GstMessage *msg, Player *player )
{
	GError *err = NULL;
	char   *dbg = NULL;

	gst_message_parse_error ( msg, &err, &dbg );

	g_critical ( "%s: %s (%s)", __func__, err->message, (dbg) ? dbg : "no details" );

	player_message_dialog ( "", err->message, GTK_MESSAGE_ERROR, player );

	g_error_free ( err );
	g_free ( dbg );

	if ( GST_ELEMENT_CAST ( player->playbin )->current_state != GST_STATE_PLAYING )
		player_set_stop ( player );
}

static void player_enc_create_elements ( Player *player )
{
	GSettings *setting = settings_init ();

	g_autofree char *enc_audio = NULL;
	g_autofree char *enc_video = NULL;
	g_autofree char *enc_muxer = NULL;

	if ( setting ) enc_audio = g_settings_get_string ( setting, "encoder-audio" );
	if ( setting ) enc_video = g_settings_get_string ( setting, "encoder-video" );
	if ( setting ) enc_muxer = g_settings_get_string ( setting, "encoder-muxer" );

	player->enc_audio = gst_element_factory_make ( ( enc_audio ) ? enc_audio : "vorbisenc", NULL );
	player->enc_video = gst_element_factory_make ( ( enc_video ) ? enc_video : "theoraenc", NULL );
	player->enc_muxer = gst_element_factory_make ( ( enc_muxer ) ? enc_muxer : "oggmux",    NULL );

	if ( setting ) g_object_unref ( setting );
}

static GstElement * player_create ( Player *player )
{
	GstElement *playbin = gst_element_factory_make ( "playbin", NULL );

	GstElement *bin_audio, *bin_video, *asink, *vsink;

	vsink     = gst_element_factory_make ( "autovideosink",     NULL );
	asink     = gst_element_factory_make ( "autoaudiosink",     NULL );

	player->videoblnc = gst_element_factory_make ( "videobalance",      NULL );
	player->equalizer = gst_element_factory_make ( "equalizer-nbands",  NULL );

	if ( !playbin )
	{
		g_critical ( "%s: playbin - not all elements could be created.", __func__ );
		return NULL;
	}

	bin_audio = gst_bin_new ( "audio-sink-bin" );
	gst_bin_add_many ( GST_BIN ( bin_audio ), player->equalizer, asink, NULL );
	gst_element_link_many ( player->equalizer, asink, NULL );

	GstPad *pad = gst_element_get_static_pad ( player->equalizer, "sink" );
	gst_element_add_pad ( bin_audio, gst_ghost_pad_new ( "sink", pad ) );
	gst_object_unref ( pad );

	bin_video = gst_bin_new ( "video-sink-bin" );
	gst_bin_add_many ( GST_BIN ( bin_video ), player->videoblnc, vsink, NULL );
	gst_element_link_many ( player->videoblnc, vsink, NULL );

	GstPad *padv = gst_element_get_static_pad ( player->videoblnc, "sink" );
	gst_element_add_pad ( bin_video, gst_ghost_pad_new ( "sink", padv ) );
	gst_object_unref ( padv );

	g_object_set ( playbin, "video-sink", bin_video, NULL );
	g_object_set ( playbin, "audio-sink", bin_audio, NULL );

	g_object_set ( playbin, "volume", VOLUME, NULL );

	GstBus *bus = gst_element_get_bus ( playbin );

	gst_bus_add_signal_watch_full ( bus, G_PRIORITY_DEFAULT );
	gst_bus_set_sync_handler ( bus, (GstBusSyncHandler)player_sync_handler, player, NULL );

	g_signal_connect ( bus, "message::eos",   G_CALLBACK ( player_msg_eos ), player );
	g_signal_connect ( bus, "message::error", G_CALLBACK ( player_msg_err ), player );
	g_signal_connect ( bus, "message::buffering", G_CALLBACK ( player_msg_buf ), player );
	g_signal_connect ( bus, "message::state-changed", G_CALLBACK ( player_msg_cng ), player );

	gst_object_unref ( bus );

	player_enc_create_elements ( player );

	return playbin;
}



static void player_msg_eos_rec ( G_GNUC_UNUSED GstBus *bus, G_GNUC_UNUSED GstMessage *msg, Player *player )
{
	player_stop_record ( player );
}

static void player_msg_err_rec ( G_GNUC_UNUSED GstBus *bus, GstMessage *msg, Player *player )
{
	player_stop_record ( player );

	GError *err = NULL;
	char   *dbg = NULL;

	gst_message_parse_error ( msg, &err, &dbg );

	g_critical ( "%s: %s (%s)", __func__, err->message, (dbg) ? dbg : "no details" );

	player_message_dialog ( "", err->message, GTK_MESSAGE_ERROR, player );

	g_error_free ( err );
	g_free ( dbg );
}

static gboolean player_update_record ( Player *player )
{
	if ( player->quit ) return FALSE;

	if ( player->pipeline_rec == NULL )
	{
		g_object_unref ( player->file_rec );
		gtk_label_set_text ( player->label_rec, "" );

		return FALSE;
	}

	GFileInfo *file_info = g_file_query_info ( player->file_rec, "standard::*", 0, NULL, NULL );

	uint64_t dsize = ( file_info ) ? g_file_info_get_attribute_uint64 ( file_info, "standard::size" ) : 0;

	g_autofree char *str_size = g_format_size ( dsize );

	const char *rec_cl = ( player->pulse ) ? "ff0000" :"2b2222";

	time_t t_cur;
	time ( &t_cur );

	if ( ( t_cur > player->t_start ) ) { time ( &player->t_start ); player->pulse = !player->pulse; }

	g_autofree char *markup = g_markup_printf_escaped ( "<span foreground=\"#%s\">   ◉   </span>%s", rec_cl, str_size );

	gtk_label_set_markup ( player->label_rec, markup );

	if ( file_info ) g_object_unref ( file_info );

	return TRUE;
}

static void player_pad_add_hls ( GstElement *element, GstPad *pad, GstElement *el )
{
	gst_element_unlink ( element, el );

	GstPad *pad_va_sink = gst_element_get_static_pad ( el, "sink" );

	if ( gst_pad_link ( pad, pad_va_sink ) == GST_PAD_LINK_OK )
		gst_object_unref ( pad_va_sink );
	else
		g_message ( "%s:: linking pad failed ", __func__ );
}
/*
static GstElement * player_create_rec_bin ( gboolean f_hls, const char *uri, const char *rec )
{
	GstElement *pipeline_rec = gst_pipeline_new ( "pipeline-record" );

	GstElement *element_src  = gst_element_make_from_uri ( GST_URI_SRC, uri, NULL, NULL );
	GstElement *element_hq   = gst_element_factory_make  ( ( f_hls ) ? "hlsdemux" : "queue2", NULL );
	GstElement *element_sinc = gst_element_factory_make  ( "filesink", NULL );

	if ( !pipeline_rec || !element_hq || !element_sinc ) return NULL;

	gst_bin_add_many ( GST_BIN ( pipeline_rec ), element_src, element_hq, element_sinc, NULL );

	gst_element_link_many ( element_src, element_hq, NULL );

	if ( found_hls )
		g_signal_connect ( element_hq, "pad-added", G_CALLBACK ( player_pad_add_hls ), element_sinc );
	else
		gst_element_link ( element_hq, element_sinc );

	if ( g_object_class_find_property ( G_OBJECT_GET_CLASS ( element_src ), "location" ) )
		g_object_set ( element_src, "location", uri, NULL );
	else
		g_object_set ( element_src, "uri", uri, NULL );

	g_object_set ( element_sinc, "location", rec, NULL );

	return pipeline_rec;
}
*/

static gboolean player_pad_check_type ( GstPad *pad, const char *type )
{
	gboolean ret = FALSE;

	GstCaps *caps = gst_pad_get_current_caps ( pad );

	const char *name = gst_structure_get_name ( gst_caps_get_structure ( caps, 0 ) );

	if ( g_str_has_prefix ( name, type ) ) ret = TRUE;

	gst_caps_unref (caps);

	return ret;
}

static void player_pad_link ( GstPad *pad, GstElement *element, const char *name )
{
	GstPad *pad_va_sink = gst_element_get_static_pad ( element, "sink" );

	if ( gst_pad_link ( pad, pad_va_sink ) == GST_PAD_LINK_OK )
		gst_object_unref ( pad_va_sink );
	else
		g_debug ( "%s:: linking demux/decode name %s video/audio pad failed ", __func__, name );
}

static void player_pad_demux_audio ( G_GNUC_UNUSED GstElement *element, GstPad *pad, GstElement *element_audio )
{
	if ( player_pad_check_type ( pad, "audio" ) ) player_pad_link ( pad, element_audio, "demux audio" );
}

static void player_pad_demux_video ( G_GNUC_UNUSED GstElement *element, GstPad *pad, GstElement *element_video )
{
	if ( player_pad_check_type ( pad, "video" ) ) player_pad_link ( pad, element_video, "demux video" );
}

static void player_pad_decode ( G_GNUC_UNUSED GstElement *element, GstPad *pad, GstElement *element_va )
{
	player_pad_link ( pad, element_va, "decode  audio / video" );
}

static GstElement * player_create_rec_bin ( gboolean f_hls, gboolean video, const char *uri, const char *rec, Player *player )
{
	GstElement *pipeline_rec = gst_pipeline_new ( "pipeline-record" );

	if ( !pipeline_rec ) return NULL;

	const char *name = ( f_hls ) ? "hlsdemux" : "queue2";

	struct rec_all { const char *name; } rec_all_n[] =
	{
		{ "souphttpsrc" }, { name        }, { "tee"          }, { "queue2"        }, { "decodebin"     },
		{ "queue2"      }, { "decodebin" }, { "audioconvert" }, { "volume"        }, { "autoaudiosink" },
		{ "queue2"      }, { "decodebin" }, { "videoconvert" }, { "autovideosink" },
		{ "queue2"      }, { "filesink"  }
	};

	GstElement *elements[ G_N_ELEMENTS ( rec_all_n ) ];

	uint8_t c = 0; for ( c = 0; c < G_N_ELEMENTS ( rec_all_n ); c++ )
	{
		if ( !video && ( c > 9 && c < 14 ) ) continue;

		if ( c == 0 )
			elements[c] = gst_element_make_from_uri ( GST_URI_SRC, uri, NULL, NULL );
		else
			elements[c] = gst_element_factory_make  ( rec_all_n[c].name, NULL );

		if ( !elements[c] )
		{
			g_critical ( "%s:: element (factory make) - %s not created. \n", __func__, rec_all_n[c].name );
			return NULL;
		}

		gst_bin_add ( GST_BIN ( pipeline_rec ), elements[c] );

		if (  c == 0 || c == 2 || c == 5 || c == 7 || c == 10 || c == 12 || c == 14 ) continue;

		gst_element_link ( elements[c-1], elements[c] );
	}

	if ( f_hls )
		g_signal_connect ( elements[1], "pad-added", G_CALLBACK ( player_pad_add_hls ), elements[2] );
	else
		gst_element_link ( elements[1], elements[2] );

	g_signal_connect ( elements[4], "pad-added", G_CALLBACK ( player_pad_demux_audio ), elements[5] );
	if ( video ) g_signal_connect ( elements[4], "pad-added", G_CALLBACK ( player_pad_demux_video ), elements[10] );

	g_signal_connect ( elements[6], "pad-added", G_CALLBACK ( player_pad_decode ), elements[7] );
	if ( video ) g_signal_connect ( elements[11], "pad-added", G_CALLBACK ( player_pad_decode ), elements[12] );

	if ( g_object_class_find_property ( G_OBJECT_GET_CLASS ( elements[0] ), "location" ) )
		g_object_set ( elements[0], "location", uri, NULL );
	else
		g_object_set ( elements[0], "uri", uri, NULL );

	gst_element_link ( elements[2], elements[14] );

	g_object_set ( elements[15], "location", rec, NULL );

	player->volume = elements[8];

	return pipeline_rec;
}

static void player_record ( Player *player )
{
	if ( player->pipeline_rec == NULL )
	{
		g_autofree char *uri = NULL;
		g_object_get ( player->playbin, "current-uri", &uri, NULL );

		if ( !uri ) return;

		int n_video = 0;
		g_object_get ( player->playbin, "n-video", &n_video, NULL );

		g_autofree char *rec_dir = NULL;
		g_autofree char *dt = helia_time_to_str ();

		// gboolean enc_b = FALSE;
		GSettings *setting = settings_init ();
		if ( setting ) rec_dir = g_settings_get_string ( setting, "rec-dir" );
		// if ( setting ) enc_b   = g_settings_get_boolean ( setting, "encoding-iptv" );

		char path[PATH_MAX] = {};

		if ( setting && rec_dir && !g_str_has_prefix ( rec_dir, "none" ) )
			sprintf ( path, "%s/Record-iptv-%s", rec_dir, dt );
		else
			sprintf ( path, "%s/Record-iptv-%s", g_get_home_dir (), dt );

		gboolean hls = FALSE;
		if ( uri && g_str_has_suffix ( uri, ".m3u8" ) ) hls = TRUE;

		player_set_stop ( player );

		player->rec_video = ( n_video > 0 ) ? TRUE : FALSE;

		// if ( enc_b )
		//	player->pipeline_rec = player_create_rec_enc_bin ( hls, player->rec_video, uri, path, player );
		// else

		player->pipeline_rec = player_create_rec_bin ( hls, player->rec_video, uri, path, player );

		if ( player->pipeline_rec == NULL ) return;

		GstBus *bus = gst_element_get_bus ( player->pipeline_rec );
		gst_bus_add_signal_watch_full ( bus, G_PRIORITY_DEFAULT );
		gst_bus_set_sync_handler ( bus, (GstBusSyncHandler)player_sync_handler, player, NULL );

		g_signal_connect ( bus, "message::eos",   G_CALLBACK ( player_msg_eos_rec ), player );
		g_signal_connect ( bus, "message::error", G_CALLBACK ( player_msg_err_rec ), player );

		gst_object_unref ( bus );
		if ( setting ) g_object_unref ( setting );

		gst_element_set_state ( player->pipeline_rec, GST_STATE_PLAYING );

		player->file_rec = g_file_new_for_path ( path );

		g_timeout_add_seconds ( 1, (GSourceFunc)player_update_record, player );
	}
	else
	{
		player_stop_record ( player );
	}
}

static void player_next_play ( char *file, Player *player )
{
	if ( player->repeat )
	{
		gst_element_set_state ( player->playbin, GST_STATE_NULL    );
		gst_element_set_state ( player->playbin, GST_STATE_PLAYING );

		return;
	}

	GtkTreeModel *model = gtk_tree_view_get_model ( player->treeview );
	int indx = gtk_tree_model_iter_n_children ( model, NULL );

	if ( indx < 2 ) { player_set_stop ( player ); return; }

	GtkTreeIter iter;
	gboolean valid, break_f = FALSE;

	for ( valid = gtk_tree_model_get_iter_first ( model, &iter ); valid;
		valid = gtk_tree_model_iter_next ( model, &iter ) )
	{
		char *data = NULL;
		gtk_tree_model_get ( model, &iter, COL_DATA,  &data, -1 );

		if ( g_str_has_suffix ( file, data ) )
		{
			if ( gtk_tree_model_iter_next ( model, &iter ) )
			{
				char *data2 = NULL;
				gtk_tree_model_get ( model, &iter, COL_DATA, &data2, -1 );

				player_stop_set_play ( data2, player );

				gtk_tree_selection_select_iter ( gtk_tree_view_get_selection ( player->treeview ), &iter );
				free ( data2 );
			}

			break_f = TRUE;
		}

		free ( data );
		if ( break_f ) break;
	}
}

static void player_treeview_row_activated ( GtkTreeView *tree_view, GtkTreePath *path, G_GNUC_UNUSED GtkTreeViewColumn *column, Player *player )
{
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model ( tree_view );

	if ( gtk_tree_model_get_iter ( model, &iter, path ) )
	{
		g_autofree char *data = NULL;
		gtk_tree_model_get ( model, &iter, COL_DATA, &data, -1 );

		player_stop_set_play ( data, player );
	}
}



static void player_enc_prop_set_audio_handler ( G_GNUC_UNUSED EncProp *ep, GObject *object, Player *player )
{
	gst_object_unref ( player->enc_audio );

	player->enc_audio = GST_ELEMENT ( object );
}
static void player_enc_prop_set_video_handler ( G_GNUC_UNUSED EncProp *ep, GObject *object, Player *player )
{
	gst_object_unref ( player->enc_video );

	player->enc_video = GST_ELEMENT ( object );
}
static void player_enc_prop_set_muxer_handler ( G_GNUC_UNUSED EncProp *ep, GObject *object, Player *player )
{
	gst_object_unref ( player->enc_muxer );

	player->enc_muxer = GST_ELEMENT ( object );
}

static void player_playlist_prop ( G_GNUC_UNUSED GtkButton *button, Player *player )
{
	GtkWindow *win_base = GTK_WINDOW ( gtk_widget_get_toplevel ( GTK_WIDGET ( player->video ) ) );

	EncProp *ep = enc_prop_new ();
	enc_prop_set_run ( win_base, player->enc_video, player->enc_audio, player->enc_muxer, TRUE, ep );

	g_signal_connect ( ep, "enc-prop-set-audio", G_CALLBACK ( player_enc_prop_set_audio_handler ), player );
	g_signal_connect ( ep, "enc-prop-set-video", G_CALLBACK ( player_enc_prop_set_video_handler ), player );
	g_signal_connect ( ep, "enc-prop-set-muxer", G_CALLBACK ( player_enc_prop_set_muxer_handler ), player );
}

static void player_playlist_repeat ( GtkButton *button, Player *player )
{
	player->repeat = !player->repeat;

	GtkImage *image = helia_create_image ( ( player->repeat ) ? "helia-set" : "helia-repeat", ICON_SIZE );

	gtk_button_set_image ( button, GTK_WIDGET ( image ) );
}

static void player_playlist_hide ( G_GNUC_UNUSED GtkButton *button, Player *player )
{
	gtk_widget_hide ( GTK_WIDGET ( player->playlist ) );
}

static void player_playlist_save ( G_GNUC_UNUSED GtkButton *button, Player *player )
{
	GtkTreeModel *model = gtk_tree_view_get_model ( player->treeview );

	int ind = gtk_tree_model_iter_n_children ( model, NULL );

	if ( ind == 0 ) return;

	GtkWindow *window = GTK_WINDOW ( gtk_widget_get_toplevel ( GTK_WIDGET ( player->video ) ) );

	g_autofree char *path = helia_save_file ( g_get_home_dir (), "playlist-001.m3u", "m3u", "*.m3u", window );

	if ( path == NULL ) return;

	helia_treeview_to_file ( path, TRUE, player->treeview );
}

static GtkBox * player_create_treeview_box ( Player *player )
{
	GtkBox *v_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_VERTICAL, 0 );
	gtk_box_set_spacing ( v_box, 5 );
	gtk_widget_set_margin_start  ( GTK_WIDGET ( v_box ), 5 );
	gtk_widget_set_margin_end    ( GTK_WIDGET ( v_box ), 5 );
	gtk_widget_set_margin_bottom ( GTK_WIDGET ( v_box ), 5 );

	GtkBox *h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_box_set_spacing ( h_box, 20 );

	player->label_buf = (GtkLabel *)gtk_label_new ( " ⇄ 0% " );
	gtk_widget_set_halign ( GTK_WIDGET ( player->label_buf ), GTK_ALIGN_START );
	gtk_box_pack_start ( h_box, GTK_WIDGET ( player->label_buf ), FALSE, FALSE, 0 );

	player->label_rec = (GtkLabel *)gtk_label_new ( "" );
	gtk_widget_set_halign ( GTK_WIDGET ( player->label_rec ), GTK_ALIGN_START );
	gtk_box_pack_end ( h_box, GTK_WIDGET ( player->label_rec ), FALSE, FALSE, 0 );

	gtk_box_pack_start ( v_box, GTK_WIDGET ( h_box ), TRUE, TRUE, 0 );

	h_box = create_treeview_box ( player->treeview );
	gtk_box_pack_start ( v_box, GTK_WIDGET ( h_box ), TRUE, TRUE, 0 );

	h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_box_set_spacing ( h_box, 5 );

	GtkButton *button = helia_create_button ( h_box, "helia-pref", "🛠", ICON_SIZE );
	g_signal_connect ( button, "clicked", G_CALLBACK ( player_playlist_prop ), player );

	button = helia_create_button ( h_box, "helia-repeat", "📡", ICON_SIZE );
	g_signal_connect ( button, "clicked", G_CALLBACK ( player_playlist_repeat ), player );

	button = helia_create_button ( h_box, "helia-save", "🖴", ICON_SIZE );
	g_signal_connect ( button, "clicked", G_CALLBACK ( player_playlist_save ), player );

	button = helia_create_button ( h_box, "helia-exit", "🞬", ICON_SIZE );
	g_signal_connect ( button, "clicked", G_CALLBACK ( player_playlist_hide ), player );

	gtk_box_pack_start ( v_box, GTK_WIDGET ( h_box ), TRUE, TRUE, 0 );

	return v_box;
}

static GtkBox * player_create_treeview_scroll ( Player *player )
{
	GtkBox *v_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_VERTICAL, 0 );
	gtk_box_set_spacing ( v_box, 5 );

	GtkScrolledWindow *scroll = (GtkScrolledWindow *)gtk_scrolled_window_new ( NULL, NULL );
	gtk_scrolled_window_set_policy ( scroll, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
	gtk_widget_set_size_request ( GTK_WIDGET ( scroll ), 220, -1 );

	Column column_n[] =
	{
		{ "Num",   "text", COL_NUM  },
		{ "Files", "text", COL_FLCH },
		{ "Data",  "text", COL_DATA }
	};

	player->treeview = create_treeview ( G_N_ELEMENTS ( column_n ), column_n );
	g_signal_connect ( player->treeview, "row-activated", G_CALLBACK ( player_treeview_row_activated ), player );

	gtk_container_add ( GTK_CONTAINER ( scroll ), GTK_WIDGET ( player->treeview ) );

	gtk_box_pack_start ( v_box, GTK_WIDGET ( scroll ), TRUE, TRUE, 0 );
	gtk_box_pack_end ( v_box, GTK_WIDGET ( player_create_treeview_box ( player ) ), FALSE, FALSE, 0 );

	return v_box;
}



static gboolean player_video_fullscreen ( GtkWindow *window )
{
	GdkWindowState state = gdk_window_get_state ( gtk_widget_get_window ( GTK_WIDGET ( window ) ) );

	if ( state & GDK_WINDOW_STATE_FULLSCREEN )
		{ gtk_window_unfullscreen ( window ); return FALSE; }
	else
		{ gtk_window_fullscreen   ( window ); return TRUE;  }

	return TRUE;
}

static gboolean player_video_press_event ( GtkDrawingArea *draw, GdkEventButton *event, Player *player )
{
	GtkWindow *window_base = GTK_WINDOW ( gtk_widget_get_toplevel ( GTK_WIDGET ( draw ) ) );

	if ( event->button == 1 )
	{
		if ( event->type == GDK_2BUTTON_PRESS )
		{
			if ( player_video_fullscreen ( GTK_WINDOW ( window_base ) ) )
			{
				gtk_widget_hide ( GTK_WIDGET ( player->playlist ) );
				gtk_widget_hide ( GTK_WIDGET ( player->slider ) );
			}
		}
	}

	if ( event->button == 2 )
	{
		if ( gtk_widget_get_visible ( GTK_WIDGET ( player->playlist ) ) )
		{
			gtk_widget_hide ( GTK_WIDGET ( player->playlist ) );
			gtk_widget_hide ( GTK_WIDGET ( player->slider ) );
		}
		else
		{
			gtk_widget_show ( GTK_WIDGET ( player->playlist ) );
			gtk_widget_show ( GTK_WIDGET ( player->slider ) );
		}
	}

	if ( event->button == 3 )
	{
		GstElement *element = player->playbin;

		gboolean play = FALSE;
		if ( GST_ELEMENT_CAST ( player->playbin )->current_state == GST_STATE_PLAYING ) play = TRUE;

		if ( player->pipeline_rec && GST_ELEMENT_CAST ( player->pipeline_rec )->current_state == GST_STATE_PLAYING ) { element = player->volume; play = TRUE; }

		ControlMp *cmp = control_mp_new ();
		control_mp_set_run ( play, element, window_base, cmp );
		g_signal_connect ( cmp, "button-click-num", G_CALLBACK ( player_clicked_handler ), player );
	}

	return TRUE;
}

static void player_video_draw_black ( GtkDrawingArea *widget, cairo_t *cr, const char *name, uint16_t size )
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
		gdk_cairo_set_source_pixbuf ( cr, pixbuf, ( width / 2  ) - ( size  / 2 ), ( heigh / 2 ) - ( size / 2 ) );
		cairo_fill (cr);
	}

	if ( pixbuf ) g_object_unref ( pixbuf );
}

static gboolean player_video_draw_check ( Player *player )
{
	if ( player->pipeline_rec && player->rec_video ) return FALSE;

	if ( GST_ELEMENT_CAST ( player->playbin )->current_state < GST_STATE_PAUSED ) return TRUE;

	int n_video = 0;
	g_object_get ( player->playbin, "n-video", &n_video, NULL );

	if ( !n_video ) return TRUE;

	return FALSE;
}

static gboolean player_video_draw ( GtkDrawingArea *widget, cairo_t *cr, Player *player )
{
	if ( player_video_draw_check ( player ) ) player_video_draw_black ( widget, cr, "helia-mp", 96 );

	return FALSE;
}

static void player_video_realize ( GtkDrawingArea *draw, Player *player )
{
	player->xid = GDK_WINDOW_XID ( gtk_widget_get_window ( GTK_WIDGET ( draw ) ) );

	if ( player->debug ) g_message ( "GDK_WINDOW_XID: %ld ", player->xid );
}

static GtkPaned * player_create_paned ( GtkBox *playlist, GtkDrawingArea *video )
{
	GtkPaned *paned = (GtkPaned *)gtk_paned_new ( GTK_ORIENTATION_HORIZONTAL );
	gtk_paned_add1 ( paned, GTK_WIDGET ( playlist ) );
	gtk_paned_add2 ( paned, GTK_WIDGET ( video    ) );

	return paned;
}



void player_treeview_append ( const char *name, const char *file, Player *player )
{
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model ( player->treeview );

	int ind = gtk_tree_model_iter_n_children ( model, NULL );

	gtk_list_store_append ( GTK_LIST_STORE ( model ), &iter );
	gtk_list_store_set    ( GTK_LIST_STORE ( model ), &iter,
				COL_NUM,  ind + 1,
				COL_FLCH, name,
				COL_DATA, file,
				-1 );

	if ( GST_ELEMENT_CAST ( player->playbin )->current_state == GST_STATE_NULL )
	{
		if ( player->debug ) g_message ( "%s:: Play %s ", __func__, file );

		player_stop_set_play ( file, player );

		gtk_tree_selection_select_iter ( gtk_tree_view_get_selection ( player->treeview ), &iter );
	}
}

static void player_video_drag_in ( G_GNUC_UNUSED GtkDrawingArea *draw, GdkDragContext *ct, G_GNUC_UNUSED int x, G_GNUC_UNUSED int y, 
	GtkSelectionData *s_data, G_GNUC_UNUSED uint info, guint32 time, Player *player )
{
	char **uris = gtk_selection_data_get_uris ( s_data );

	uint c = 0; for ( c = 0; uris[c] != NULL; c++ )
	{
		char *path = helia_uri_get_path ( uris[c] );

		helia_add_file ( path, player );

		free ( path );
	}

	g_strfreev ( uris );
	gtk_drag_finish ( ct, TRUE, FALSE, time );
}



static gboolean player_slider_refresh ( Player *player )
{
	if ( player->quit ) return FALSE;

	GstElement *element = player->playbin;
	if ( player->pipeline_rec ) element = player->pipeline_rec;

	if ( GST_ELEMENT_CAST ( element )->current_state == GST_STATE_NULL    ) return TRUE;
	if ( GST_ELEMENT_CAST ( element )->current_state  < GST_STATE_PLAYING ) return TRUE;

	gboolean dur_b = FALSE;
	gint64 duration = 0, current = 0;

	if ( gst_element_query_position ( element, GST_FORMAT_TIME, &current ) )
	{
		if ( gst_element_query_duration ( element, GST_FORMAT_TIME, &duration ) ) dur_b = TRUE;

			if ( dur_b && duration / GST_SECOND > 0 )
			{
				if ( current / GST_SECOND < duration / GST_SECOND )
				{
					slider_update ( player->slider, (double)duration / GST_SECOND, (double)current / GST_SECOND );

					slider_set_data ( player->slider, current, 8, duration, 10, TRUE );
				}
			}
			else
			{
				slider_set_data ( player->slider, current, 8, -1, 10, FALSE );
			}
	}

	return TRUE;
}

static void player_video_scroll_new_pos ( gint64 set_pos, gboolean up_dwn, Player *player )
{
	gboolean dur_b = FALSE;
	gint64 current = 0, duration = 0, new_pos = 0, skip = (gint64)( set_pos * GST_SECOND );

	if ( gst_element_query_position ( player->playbin, GST_FORMAT_TIME, &current ) )
	{
		if ( gst_element_query_duration ( player->playbin, GST_FORMAT_TIME, &duration ) ) dur_b = TRUE;

		if ( !dur_b || duration / GST_SECOND < 1 ) return;

		if ( up_dwn ) new_pos = ( duration > ( current + skip ) ) ? ( current + skip ) : duration;

		if ( !up_dwn ) new_pos = ( current > skip ) ? ( current - skip ) : 0;

		gst_element_seek_simple ( player->playbin, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT, new_pos );
	}
}

static gboolean player_video_scroll_even ( G_GNUC_UNUSED GtkDrawingArea *widget, GdkEventScroll *evscroll, Player *player )
{
	if ( GST_ELEMENT_CAST ( player->playbin )->current_state == GST_STATE_NULL ) return TRUE;

	gint64 skip = 20;

	gboolean up_dwn = TRUE;
	if ( evscroll->direction == GDK_SCROLL_DOWN ) up_dwn = FALSE;
	if ( evscroll->direction == GDK_SCROLL_UP   ) up_dwn = TRUE;

	if ( evscroll->direction == GDK_SCROLL_DOWN || evscroll->direction == GDK_SCROLL_UP )
		player_video_scroll_new_pos ( skip, up_dwn, player );

	return TRUE;
}

static void player_slider_seek_changed ( GtkRange *range, Player *player )
{
	if ( GST_ELEMENT_CAST ( player->playbin )->current_state == GST_STATE_NULL ) return;

	double value = gtk_range_get_value ( GTK_RANGE (range) );

	gst_element_seek_simple ( player->playbin, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT, (gint64)( value * GST_SECOND ) );

	slider_set_data ( player->slider, (gint64)( value * GST_SECOND ), 8, -1, 10, TRUE );
}

static void player_step_pos ( guint64 am, Player *player )
{
	gst_element_send_event ( player->playbin, gst_event_new_step ( GST_FORMAT_BUFFERS, am, 1.0, TRUE, FALSE ) );

	gint64 current = 0;

	if ( gst_element_query_position ( player->playbin, GST_FORMAT_TIME, &current ) )
	{
		slider_set_data ( player->slider, current, 7, -1, 10, TRUE );
	}
}
static void player_step_frame ( Player *player )
{
	if ( GST_ELEMENT_CAST ( player->playbin )->current_state == GST_STATE_NULL ) return;

	int n_video = 0;
	g_object_get ( player->playbin, "n-video", &n_video, NULL );

	if ( n_video == 0 ) return;

	if ( GST_ELEMENT_CAST ( player->playbin )->current_state == GST_STATE_PLAYING )
		gst_element_set_state ( player->playbin, GST_STATE_PAUSED );

	player_step_pos ( 1, player );
}


typedef struct _FuncAction FuncAction;

struct _FuncAction
{
	void (*f)();
	const char *func_name;

	uint mod_key;
	uint gdk_key;
};

static void player_action_play ( G_GNUC_UNUSED GSimpleAction *sl, G_GNUC_UNUSED GVariant *pm, Player *player )
{
	if ( player->run ) player_set_pause ( player );
}

static void player_action_step ( G_GNUC_UNUSED GSimpleAction *sl, G_GNUC_UNUSED GVariant *pm, Player *player )
{
	if ( player->run ) player_step_frame ( player );
}

static void player_action_dir ( G_GNUC_UNUSED GSimpleAction *sl, G_GNUC_UNUSED GVariant *pm, Player *player )
{
	if ( player->run )
	{
		GtkWindow *window = GTK_WINDOW ( gtk_widget_get_toplevel ( GTK_WIDGET ( player->video ) ) );

		g_autofree char *path = helia_open_dir ( g_get_home_dir (), window );

		if ( path == NULL ) return;

		helia_add_dir ( path, player );
	}
}

static void player_action_files ( G_GNUC_UNUSED GSimpleAction *sl, G_GNUC_UNUSED GVariant *pm, Player *player )
{
	if ( player->run )
	{
		GtkWindow *window = GTK_WINDOW ( gtk_widget_get_toplevel ( GTK_WIDGET ( player->video ) ) );

		GSList *files = helia_open_files ( g_get_home_dir (), window );

		if ( files == NULL ) return;

		while ( files != NULL )
		{
			helia_add_file ( files->data, player );
			files = files->next;
		}

		g_slist_free_full ( files, (GDestroyNotify) g_free );
	}
}

static void player_action_net ( G_GNUC_UNUSED GSimpleAction *sl, G_GNUC_UNUSED GVariant *pm, Player *player )
{
	if ( player->run )
	{
		GtkWindow *window = GTK_WINDOW ( gtk_widget_get_toplevel ( GTK_WIDGET ( player->video ) ) );

		helia_open_net ( window, player );
	}
}

static void player_action_slider ( G_GNUC_UNUSED GSimpleAction *sl, G_GNUC_UNUSED GVariant *pm, Player *player )
{
	if ( !player->run ) return;

	if ( gtk_widget_get_visible ( GTK_WIDGET ( player->slider ) ) )
		gtk_widget_hide ( GTK_WIDGET ( player->slider ) );
	else
		gtk_widget_show ( GTK_WIDGET ( player->slider ) );
}

static void player_action_list ( G_GNUC_UNUSED GSimpleAction *sl, G_GNUC_UNUSED GVariant *pm, Player *player )
{
	if ( !player->run ) return;

	player_set_playlist ( player );
}

static FuncAction func_action_n[] =
{
	{ player_action_dir,    "add_dir",     GDK_CONTROL_MASK, GDK_KEY_D },
	{ player_action_files,  "add_files",   GDK_CONTROL_MASK, GDK_KEY_O },
	{ player_action_net,    "add_net",     GDK_CONTROL_MASK, GDK_KEY_L },
	{ player_action_play,   "play_paused", 0, GDK_KEY_space  },
	{ player_action_step,   "play_step",   0, GDK_KEY_period },
	{ player_action_slider, "slider",      GDK_CONTROL_MASK, GDK_KEY_Z },
	{ player_action_list,   "playlist",    GDK_CONTROL_MASK, GDK_KEY_H }
};

static void app_add_accelerator ( GtkApplication *app, uint i )
{
	g_autofree char *accel_name = gtk_accelerator_name ( func_action_n[i].gdk_key, func_action_n[i].mod_key );

	const char *accel_str[] = { accel_name, NULL };

	g_autofree char *text = g_strconcat ( "app.", func_action_n[i].func_name, NULL );

	gtk_application_set_accels_for_action ( app, text, accel_str );
}

static void create_gaction_entry ( GtkApplication *app, Player *player )
{
	GActionEntry entries[ G_N_ELEMENTS ( func_action_n ) ];

	uint8_t i = 0; for ( i = 0; i < G_N_ELEMENTS ( func_action_n ); i++ )
	{
		entries[i].name           = func_action_n[i].func_name;
		entries[i].activate       = func_action_n[i].f;
		entries[i].parameter_type = NULL;
		entries[i].state          = NULL;

		app_add_accelerator ( app, i );
	}

	g_action_map_add_action_entries ( G_ACTION_MAP ( app ), entries, G_N_ELEMENTS ( entries ), player );
}

void player_add_accel ( GtkApplication *app, Player *player )
{
	create_gaction_entry ( app, player );
}



static void player_show_cursor ( GtkDrawingArea *draw, gboolean show_cursor )
{
	GdkWindow *window = gtk_widget_get_window ( GTK_WIDGET ( draw ) );

	GdkCursor *cursor = gdk_cursor_new_for_display ( gdk_display_get_default (), ( show_cursor ) ? GDK_ARROW : GDK_BLANK_CURSOR );

	gdk_window_set_cursor ( window, cursor );

	g_object_unref (cursor);
}

static gboolean player_video_notify_event ( GtkDrawingArea *draw, G_GNUC_UNUSED GdkEventMotion *event, Player *player )
{
	if ( player->quit ) return GDK_EVENT_STOP;

	time ( &player->t_hide );

	player_show_cursor ( draw, TRUE );

	return GDK_EVENT_STOP;
}

static gboolean player_video_hide_cursor ( Player *player )
{
	if ( player->quit ) return FALSE;
	if ( !player->run ) return TRUE;

	time_t t_cur;
	time ( &t_cur );

	if ( ( t_cur - player->t_hide < 2 ) ) return TRUE;

	gboolean show = TRUE;
	if ( player->pipeline_rec && player->rec_video ) show = FALSE;

	int n_video = 0;
	g_object_get ( player->playbin, "n-video", &n_video, NULL );
	if ( GST_ELEMENT_CAST ( player->playbin )->current_state >= GST_STATE_PAUSED && n_video > 0 ) show = FALSE;

	GtkWindow *window = GTK_WINDOW ( gtk_widget_get_toplevel ( GTK_WIDGET ( player->video ) ) );

	if ( !gtk_window_is_active ( window ) ) player_show_cursor ( player->video, TRUE ); else player_show_cursor ( player->video, show );

	return TRUE;
}



static void player_init ( Player *player )
{
	player->run  = FALSE;
	player->quit = FALSE;
	player->repeat = FALSE;
	player->opacity = OPACITY;
	player->debug = ( g_getenv ( "DVB_DEBUG" ) ) ? TRUE : FALSE;

	GtkBox *box = GTK_BOX ( player );
	gtk_orientable_set_orientation ( GTK_ORIENTABLE ( box ), GTK_ORIENTATION_VERTICAL );
	gtk_box_set_spacing ( box, 3 );

	player->pipeline_rec = NULL;
	player->playbin = player_create ( player );

	player->playlist = player_create_treeview_scroll ( player );

	player->video = (GtkDrawingArea *)gtk_drawing_area_new ();
	gtk_widget_set_events ( GTK_WIDGET ( player->video ), GDK_BUTTON_PRESS_MASK | GDK_SCROLL_MASK | GDK_POINTER_MOTION_MASK );

	g_signal_connect ( player->video, "draw", G_CALLBACK ( player_video_draw ), player );
	g_signal_connect ( player->video, "realize", G_CALLBACK ( player_video_realize ), player );
	g_signal_connect ( player->video, "scroll-event", G_CALLBACK ( player_video_scroll_even ), player );

	g_signal_connect ( player->video, "button-press-event",  G_CALLBACK ( player_video_press_event  ), player );
	g_signal_connect ( player->video, "motion-notify-event", G_CALLBACK ( player_video_notify_event ), player );

	gtk_drag_dest_set ( GTK_WIDGET ( player->video ), GTK_DEST_DEFAULT_ALL, NULL, 0, GDK_ACTION_COPY );
	gtk_drag_dest_add_uri_targets  ( GTK_WIDGET ( player->video ) );
	g_signal_connect  ( player->video, "drag-data-received", G_CALLBACK ( player_video_drag_in ), player );

	GtkPaned *paned = player_create_paned ( player->playlist, player->video );
	gtk_box_pack_start ( box, GTK_WIDGET ( paned ), TRUE, TRUE, 0 );

	player->slider = slider_new ();
	g_timeout_add ( 100, (GSourceFunc)player_slider_refresh, player );

	GtkScale *scale = slider_get_scale ( player->slider );
	ulong signal_id = g_signal_connect ( scale, "value-changed", G_CALLBACK ( player_slider_seek_changed ), player );

	slider_set_signal_id ( player->slider, signal_id );

	gtk_box_pack_end ( box, GTK_WIDGET ( player->slider ), FALSE, FALSE, 0 );

	g_timeout_add_seconds ( 2, (GSourceFunc)player_video_hide_cursor, player );
}

void player_quit ( Player *player )
{
	player->quit = TRUE;

	g_object_set ( player->playbin, "mute", FALSE, NULL );
	gst_element_set_state ( player->playbin, GST_STATE_NULL );

	gst_object_unref ( player->playbin );
}

void player_run_status ( uint16_t opacity, gboolean status, Player *player )
{
	player->run = status;
	player->opacity = opacity;
}

static void player_finalize ( GObject *object )
{
	G_OBJECT_CLASS (player_parent_class)->finalize (object);
}

static void player_class_init ( PlayerClass *class )
{
	G_OBJECT_CLASS (class)->finalize = player_finalize;

	g_signal_new ( "button-clicked", G_TYPE_FROM_CLASS ( class ), G_SIGNAL_RUN_FIRST,
		0, NULL, NULL, g_cclosure_marshal_VOID__STRING, G_TYPE_NONE, 1, G_TYPE_STRING );

	g_signal_new ( "power-set", G_TYPE_FROM_CLASS ( class ), G_SIGNAL_RUN_FIRST,
		0, NULL, NULL, g_cclosure_marshal_VOID__STRING, G_TYPE_NONE, 1, G_TYPE_BOOLEAN );
}

Player * player_new ( void )
{
	return g_object_new ( PLAYER_TYPE_BOX, NULL );
}
