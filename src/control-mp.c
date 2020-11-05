/*
* Copyright 2020 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-3
* file:///usr/share/common-licenses/GPL-3
* http://www.gnu.org/licenses/gpl-3.0.html
*/

#include "control-mp.h"
#include "default.h"
#include "button.h"
#include "settings.h"

struct _ControlMp
{
	GtkWindow parent_instance;

	GtkButton *button_pause;

	GtkVolumeButton *volbutton;

	GstElement *element;

	uint16_t opacity;
	uint16_t icon_size;
};

G_DEFINE_TYPE ( ControlMp, control_mp, GTK_TYPE_WINDOW )

static void control_button_set_icon ( GtkButton *button, const char *name, uint icon_size )
{
	GdkPixbuf *pixbuf = gtk_icon_theme_load_icon ( gtk_icon_theme_get_default (), 
		name, (int)icon_size, GTK_ICON_LOOKUP_USE_BUILTIN, NULL );

	GtkImage *image   = (GtkImage *)gtk_image_new_from_pixbuf ( pixbuf );
	gtk_button_set_image ( button, GTK_WIDGET ( image ) );

	if ( pixbuf ) g_object_unref ( pixbuf );
}

void control_mp_set_run ( gboolean play, GstElement *element, GtkWindow *win_base, ControlMp *cmp )
{
	cmp->element = element;

	double value = VOLUME;
	g_object_get ( cmp->element, "volume", &value, NULL );
	gtk_scale_button_set_value ( GTK_SCALE_BUTTON ( cmp->volbutton ), value );

	GtkWindow *window = GTK_WINDOW ( cmp );

	gtk_window_set_transient_for ( window, win_base );

	gtk_widget_show_all ( GTK_WIDGET ( window ) );
	gtk_widget_set_opacity ( GTK_WIDGET ( window ), ( (float)cmp->opacity / 100 ) );

	gboolean icon = helia_check_icon_theme ( "helia-play" );

	if ( icon )
		control_button_set_icon ( cmp->button_pause, ( play ) ? "helia-pause" : "helia-play", cmp->icon_size );
	else
		gtk_button_set_label ( cmp->button_pause, ( play ) ? "⏸" : "⏵" );
}

static void control_mp_signal_handler_num ( GtkButton *button, ControlMp *cmp )
{
	const char *name = gtk_widget_get_name ( GTK_WIDGET ( button ) );

	g_signal_emit_by_name ( cmp, "button-click-num", atoi ( name ) );
}

static void control_mp_volume_changed ( G_GNUC_UNUSED GtkScaleButton *button, double value, ControlMp *cmp )
{
	g_object_set ( cmp->element, "volume", value, NULL );
}

static void control_mp_clicked_stop ( G_GNUC_UNUSED GtkButton *button, ControlMp *cmp )
{
	gboolean icon = helia_check_icon_theme ( "helia-play" );

	if ( icon )
		control_button_set_icon ( cmp->button_pause, "helia-play", cmp->icon_size );
	else
		gtk_button_set_label ( cmp->button_pause, "⏵" );
}

static gboolean control_mp_clicked_pause_timeout ( ControlMp *cmp )
{
	gboolean play = FALSE;
	if ( GST_ELEMENT_CAST ( cmp->element )->current_state == GST_STATE_PLAYING ) play = TRUE;

	gboolean icon = helia_check_icon_theme ( "helia-play" );

	if ( icon )
		control_button_set_icon ( cmp->button_pause, ( play ) ? "helia-pause" : "helia-play", cmp->icon_size );
	else
		gtk_button_set_label ( cmp->button_pause, ( play ) ? "⏸" : "⏵" );

	return FALSE;
}

static void control_mp_clicked_pause ( G_GNUC_UNUSED GtkButton *button, ControlMp *cmp )
{
	g_timeout_add ( 1000, (GSourceFunc)control_mp_clicked_pause_timeout, cmp );
}

static void control_mp_init ( ControlMp *cmp )
{
	cmp->opacity = OPACITY;
	cmp->icon_size = ICON_SIZE;

	GSettings *setting = settings_init ();
	if ( setting ) cmp->opacity   = (uint16_t)g_settings_get_uint ( setting, "opacity-panel" );
	if ( setting ) cmp->icon_size = (uint16_t)g_settings_get_uint ( setting, "icon-size" );

	GtkWindow *window = GTK_WINDOW ( cmp );

	gtk_window_set_decorated ( window, FALSE  );
	gtk_window_set_title ( window, "" );
	gtk_window_set_modal ( window, TRUE );
	gtk_window_set_icon_name ( window, DEF_ICON );
	gtk_window_set_position  ( window, GTK_WIN_POS_CENTER_ON_PARENT );
	gtk_window_set_default_size ( window, 400, -1 );

	GtkBox *m_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_VERTICAL, 0 );
	gtk_box_set_spacing ( m_box, 5 );

	GtkBox *b_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_box_set_spacing ( b_box, 5 );

	GtkBox *v_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_VERTICAL, 0 );
	gtk_box_set_spacing ( v_box, 5 );

	GtkBox *h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_box_set_spacing ( h_box, 5 );

	uint8_t BN = 5;

	const char *icons_a[] = { "🎬", "", "☰", "☰", "🔇" };
	const char *name_icons_a[] = { "helia-mp", "helia-editor", "helia-eqa", "helia-eqv", "helia-muted" };
	const gboolean swapped_a[] = { TRUE, FALSE, TRUE, TRUE, FALSE };

	uint8_t c = 0; for ( c = 0; c < BN; c++ )
	{
		GtkButton *button = helia_create_button ( h_box, name_icons_a[c], icons_a[c], cmp->icon_size );

		char name[20];
		sprintf ( name, "%u", c );
		gtk_widget_set_name ( GTK_WIDGET ( button ), name );

		g_signal_connect ( button, "clicked", G_CALLBACK ( control_mp_signal_handler_num ), cmp );
		if ( swapped_a[c] ) g_signal_connect_swapped ( button, "clicked", G_CALLBACK ( gtk_widget_destroy ), window );
	}

	gtk_box_pack_start ( v_box, GTK_WIDGET ( h_box ), FALSE, FALSE, 0 );

	h_box = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_box_set_spacing ( h_box, 5 );

	const char *icons_b[] = { "⏵", "⏹", "⏺", "⏼", "🞬" };
	const char *name_icons_b[] = { "helia-play", "helia-stop", "helia-record", "helia-info", "helia-exit" };
	const gboolean swapped_b[] = { FALSE, FALSE, FALSE, TRUE, TRUE };

	for ( c = 0; c < BN; c++ )
	{
		GtkButton *button = helia_create_button ( h_box, name_icons_b[c], icons_b[c], cmp->icon_size );
		if ( c == 0 ) cmp->button_pause = button;

		char name[20];
		sprintf ( name, "%u", c + BN );
		gtk_widget_set_name ( GTK_WIDGET ( button ), name );

		g_signal_connect ( button, "clicked", G_CALLBACK ( control_mp_signal_handler_num ), cmp );

		if ( c == 0 ) g_signal_connect ( button, "clicked", G_CALLBACK ( control_mp_clicked_pause ), cmp );
		if ( c == 1 ) g_signal_connect ( button, "clicked", G_CALLBACK ( control_mp_clicked_stop  ), cmp );

		if ( swapped_b[c] ) g_signal_connect_swapped ( button, "clicked", G_CALLBACK ( gtk_widget_destroy ), window );
	}

	gtk_box_pack_start ( v_box, GTK_WIDGET ( h_box ), FALSE, FALSE, 0 );
	gtk_box_pack_start ( b_box, GTK_WIDGET ( v_box ), TRUE, TRUE, 0 );

	cmp->volbutton = (GtkVolumeButton *)gtk_volume_button_new ();
	gtk_button_set_relief ( GTK_BUTTON ( cmp->volbutton ), GTK_RELIEF_NORMAL );
	g_signal_connect ( cmp->volbutton, "value-changed", G_CALLBACK ( control_mp_volume_changed ), cmp );

	gtk_box_pack_end ( b_box, GTK_WIDGET ( cmp->volbutton ), FALSE, FALSE, 0 );

	gtk_box_pack_start ( m_box, GTK_WIDGET ( b_box ), FALSE, FALSE, 0 );

	gtk_container_set_border_width ( GTK_CONTAINER ( m_box ), 10 );
	gtk_container_add ( GTK_CONTAINER ( window ), GTK_WIDGET ( m_box ) );

	if ( setting ) g_object_unref ( setting );
}

static void control_mp_finalize ( GObject *object )
{
	G_OBJECT_CLASS (control_mp_parent_class)->finalize (object);
}

static void control_mp_class_init ( ControlMpClass *class )
{
	G_OBJECT_CLASS (class)->finalize = control_mp_finalize;

	g_signal_new ( "button-click-num", G_TYPE_FROM_CLASS ( class ), G_SIGNAL_RUN_FIRST,
		0, NULL, NULL, g_cclosure_marshal_VOID__STRING, G_TYPE_NONE, 1, G_TYPE_UINT );
}

ControlMp * control_mp_new ( void )
{
	return g_object_new ( CONTROLMP_TYPE_WINDOW, NULL );
}
