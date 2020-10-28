/*
* Copyright 2020 Stepan Perun
* This program is free software.
* 
* License: Gnu General Public License GPL-3
* file:///usr/share/common-licenses/GPL-3
* http://www.gnu.org/licenses/gpl-3.0.html
*/

#include "helia-eqa.h"
#include "default.h"
#include "button.h"

#define BAND_N 10

enum EQA_N
{
	NUM_L,
	NUM_F,
	NUM_B,
	NUMS
};

typedef struct _EQAudio EQAudio;

struct _EQAudio
{
	GtkScale *scale_gfb_n[BAND_N];
};


static void helia_eqa_all_changed ( GtkRange *range, gpointer data, const char *desc )
{
	GMutex mutex;
	g_mutex_init ( &mutex );
	g_mutex_lock ( &mutex );

	GstObject *band = GST_OBJECT ( data );
	double value = gtk_range_get_value ( range );

	g_object_set ( band, desc, value, NULL );

	g_mutex_unlock ( &mutex );
	g_mutex_clear  ( &mutex );
}

static void helia_eqa_gain_changed ( GtkRange *range, gpointer data )
{
	helia_eqa_all_changed ( range, data, "gain" );
}
static void helia_eqa_freq_changed ( GtkRange *range, gpointer data )
{
	helia_eqa_all_changed ( range, data, "freq" );
}
static void helia_eqa_band_changed ( GtkRange *range, gpointer data )
{
	helia_eqa_all_changed ( range, data, "bandwidth" );
}

static void helia_eqa_default ( G_GNUC_UNUSED GtkButton *button, EQAudio **eqa_gfb )
{
	double g = 0.0, f = 30, b = 20;

	uint8_t i = 0; for ( i = 0; i < BAND_N; i++ )
	{
		gtk_range_set_value ( GTK_RANGE ( eqa_gfb[0]->scale_gfb_n[i] ), g );
		gtk_range_set_value ( GTK_RANGE ( eqa_gfb[1]->scale_gfb_n[i] ), f );
		gtk_range_set_value ( GTK_RANGE ( eqa_gfb[2]->scale_gfb_n[i] ), b );

		f = f * 2; b = b * 2;
	}
}

static void helia_eqa_create_label ( GtkBox *vbox )
{
	const char *name[] = { "Ⓛ   dB", "Ⓕ   Hz", "Ⓑ   Hz" };

	GtkBox *hboxl = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	GtkLabel *label;

	uint8_t i = 0; for ( i = 0; i < G_N_ELEMENTS ( name ); i++ )
	{
		label = (GtkLabel *)gtk_label_new ( name[i] );
		gtk_box_pack_start ( hboxl, GTK_WIDGET ( label ), TRUE, TRUE, 10 );
	}

	gtk_box_pack_start ( vbox,  GTK_WIDGET ( hboxl  ), FALSE, FALSE,  0 );
}

static GtkScale * helia_eqa_create_scale_g_f_b ( GtkBox *scales_hbox, double g_f_b, double min_v, double max_v, double step_v )
{
	GtkScale *widget =  (GtkScale *)gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, min_v, max_v, step_v );
	gtk_range_set_value ( GTK_RANGE ( widget ), g_f_b );

	gtk_widget_set_size_request ( GTK_WIDGET ( widget ), 100, -1 );
	gtk_box_pack_start ( scales_hbox, GTK_WIDGET ( widget ), TRUE, TRUE, 5 );

	gtk_scale_set_draw_value ( GTK_SCALE ( widget ), 1 );

	return widget;
}

static void helia_eqa_win_quit ( G_GNUC_UNUSED GtkWindow *window, EQAudio **eqa_gfb )
{
	uint8_t c = 0;

	for ( c = 0; c < NUMS; c++ ) free ( eqa_gfb[c] );

	free ( eqa_gfb );
}

void helia_eqa_win ( uint16_t opacity, GtkWindow *win_base, GstElement *element )
{
	GtkWindow *window = (GtkWindow *)gtk_window_new ( GTK_WINDOW_TOPLEVEL );
	gtk_window_set_title ( window, "Audio EQ" );
	gtk_window_set_modal ( window, TRUE );
	gtk_window_set_transient_for ( window, win_base );
	gtk_window_set_icon_name ( window, DEF_ICON );
	gtk_window_set_position  ( window, GTK_WIN_POS_CENTER_ON_PARENT );

	uint8_t c = 0;
	GtkBox *vbox_main, *vbox, *h_box;

	EQAudio **eqa_gfb = g_malloc0 ( NUMS * sizeof (EQAudio *) );

	for ( c = 0; c < NUMS; c++ ) eqa_gfb[c] = g_new0 ( EQAudio, 1 );

	vbox_main = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_VERTICAL, 0 );
	gtk_widget_set_margin_top   ( GTK_WIDGET ( vbox_main ), 10 );
	gtk_widget_set_margin_start ( GTK_WIDGET ( vbox_main ), 10 );
	gtk_widget_set_margin_end   ( GTK_WIDGET ( vbox_main ), 10 );

	vbox = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_VERTICAL, 0 );

	helia_eqa_create_label ( vbox_main );

	g_object_set ( G_OBJECT ( element ), "num-bands", BAND_N, NULL );

	for ( c = 0; c < BAND_N; c++ )
	{
		double freq, bw, gain;
		GObject *band = gst_child_proxy_get_child_by_index ( GST_CHILD_PROXY ( element ), c );

		g_assert ( band != NULL );
		g_object_get ( band, "gain",      &gain, NULL );
		g_object_get ( band, "freq",      &freq, NULL );
		g_object_get ( band, "bandwidth", &bw,   NULL );

		GtkBox *scales_hbox = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );

		eqa_gfb[0]->scale_gfb_n[c] = helia_eqa_create_scale_g_f_b ( scales_hbox, gain, -24.0, 12.0, 1.0 );
		g_signal_connect ( G_OBJECT ( eqa_gfb[0]->scale_gfb_n[c] ), "value-changed", G_CALLBACK ( helia_eqa_gain_changed ), (gpointer)band );

		eqa_gfb[1]->scale_gfb_n[c] = helia_eqa_create_scale_g_f_b ( scales_hbox, freq, 10.0, 20000.0, 5.0 );
		g_signal_connect ( G_OBJECT ( eqa_gfb[1]->scale_gfb_n[c] ), "value-changed", G_CALLBACK ( helia_eqa_freq_changed ), (gpointer)band );

		eqa_gfb[2]->scale_gfb_n[c] = helia_eqa_create_scale_g_f_b ( scales_hbox, bw, 10.0, 20000.0, 10.0 );
		g_signal_connect ( G_OBJECT ( eqa_gfb[2]->scale_gfb_n[c] ), "value-changed", G_CALLBACK ( helia_eqa_band_changed ), (gpointer)band );

		gtk_box_pack_start ( vbox, GTK_WIDGET ( scales_hbox ), TRUE, TRUE, 0 );
	}

	gtk_box_pack_start ( vbox_main, GTK_WIDGET ( vbox ), TRUE, TRUE, 0 );

	h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_box_set_spacing ( h_box, 10 );

	GtkButton *button = helia_create_button ( h_box, "helia-clear", "🗑", ICON_SIZE );
	g_signal_connect ( button, "clicked", G_CALLBACK ( helia_eqa_default ), eqa_gfb );

	button = helia_create_button ( h_box, "helia-exit", "🞬", ICON_SIZE );
	g_signal_connect_swapped ( button, "clicked", G_CALLBACK ( gtk_widget_destroy ), window );

	gtk_box_pack_start ( vbox_main, GTK_WIDGET ( h_box ), FALSE, FALSE, 10 );

	gtk_container_add ( GTK_CONTAINER ( window ), GTK_WIDGET ( vbox_main ) );

	gtk_widget_show_all ( GTK_WIDGET ( window ) );
	gtk_widget_set_opacity ( GTK_WIDGET ( window ), (float)opacity / 100 );

	g_signal_connect ( window, "destroy", G_CALLBACK ( helia_eqa_win_quit ), eqa_gfb );
}
