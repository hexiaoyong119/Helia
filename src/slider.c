/*
* Copyright 2020 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-3
* file:///usr/share/common-licenses/GPL-3
* http://www.gnu.org/licenses/gpl-3.0.html
*/

#include "slider.h"

struct _Slider
{
	GtkBox parent_instance;

	GtkScale *slider;
	GtkLabel *lab_pos;
	GtkLabel *lab_dur;

	ulong slider_signal_id;
};

G_DEFINE_TYPE ( Slider, slider, GTK_TYPE_BOX )

static void slider_label_set_text ( GtkLabel *label, gint64 pos_dur, uint8_t digits )
{
	char buf[100], text[100];
	sprintf ( buf, "%" GST_TIME_FORMAT, GST_TIME_ARGS ( pos_dur ) );
	snprintf ( text, strlen ( buf ) - digits + 1, "%s", buf );

	gtk_label_set_text ( label, text );
}

GtkScale * slider_get_scale ( Slider *hsl )
{
	return hsl->slider;
}

void slider_set_signal_id ( Slider *hsl, ulong signal_id )
{
	hsl->slider_signal_id = signal_id;
}

void slider_set_data ( Slider *hsl, gint64 pos, uint8_t digits_pos, gint64 dur, uint8_t digits_dur, gboolean sensitive )
{
	slider_label_set_text ( hsl->lab_pos, pos, digits_pos );

	if ( dur > -1 ) slider_label_set_text ( hsl->lab_dur, dur, digits_dur );

	gtk_widget_set_sensitive ( GTK_WIDGET ( hsl ), sensitive );
}

void slider_update ( Slider *hsl, double range, double value )
{
	g_signal_handler_block   ( hsl->slider, hsl->slider_signal_id );

		gtk_range_set_range  ( GTK_RANGE ( hsl->slider ), 0, range );
		gtk_range_set_value  ( GTK_RANGE ( hsl->slider ),    value );

	g_signal_handler_unblock ( hsl->slider, hsl->slider_signal_id );
}

void slider_clear_all ( Slider *hsl )
{
	slider_update ( hsl, 120*60, 0 );

	gtk_label_set_text ( hsl->lab_pos, "0:00:00" );
	gtk_label_set_text ( hsl->lab_dur, "0:00:00" );

	gtk_widget_set_sensitive ( GTK_WIDGET ( hsl ), FALSE );
}

static void slider_init ( Slider *hsl )
{
	gtk_orientable_set_orientation ( GTK_ORIENTABLE ( hsl ), GTK_ORIENTATION_HORIZONTAL );

	hsl->lab_pos = (GtkLabel *)gtk_label_new ( "0:00:00" );
	hsl->lab_dur = (GtkLabel *)gtk_label_new ( "0:00:00" );

	hsl->slider  = (GtkScale *)gtk_scale_new_with_range ( GTK_ORIENTATION_HORIZONTAL, 0, 120*60, 1 );

	gtk_scale_set_draw_value ( hsl->slider, 0 );
	gtk_range_set_value ( GTK_RANGE ( hsl->slider ), 0 );

	gtk_widget_set_margin_start ( GTK_WIDGET ( GTK_BOX ( hsl ) ), 10 );
	gtk_widget_set_margin_end   ( GTK_WIDGET ( GTK_BOX ( hsl ) ), 10 );
	gtk_box_set_spacing ( GTK_BOX ( hsl ), 5 );

	gtk_box_pack_start ( GTK_BOX ( hsl ), GTK_WIDGET ( hsl->lab_pos ), FALSE, FALSE, 0 );
	gtk_box_pack_start ( GTK_BOX ( hsl ), GTK_WIDGET ( hsl->slider  ), TRUE,  TRUE,  0 );
	gtk_box_pack_start ( GTK_BOX ( hsl ), GTK_WIDGET ( hsl->lab_dur ), FALSE, FALSE, 0 );

	gtk_widget_set_sensitive ( GTK_WIDGET ( hsl ), FALSE );
}

static void slider_finalize ( GObject *object )
{
	G_OBJECT_CLASS (slider_parent_class)->finalize (object);
}

static void slider_class_init ( SliderClass *class )
{
	G_OBJECT_CLASS (class)->finalize = slider_finalize;
}

Slider * slider_new (void)
{
	return g_object_new ( SLIDER_TYPE_BOX, NULL );
}

