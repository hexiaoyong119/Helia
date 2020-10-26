/*
* Copyright 2020 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-3
* file:///usr/share/common-licenses/GPL-3
* http://www.gnu.org/licenses/gpl-3.0.html
*/

#include "level.h"

#include <time.h>

struct _Level
{
	GtkBox parent_instance;

	GtkLabel *sgn_snr;
	GtkProgressBar *bar_sgn;
	GtkProgressBar *bar_snr;

	time_t t_start;
	gboolean pulse;
};

G_DEFINE_TYPE ( Level, level, GTK_TYPE_BOX )

void level_set_sgn_snr ( uint8_t sgl, uint8_t snr, gboolean lock, gboolean rec, Level *level )
{
	gtk_progress_bar_set_fraction ( level->bar_sgn, (double)sgl / 100 );
	gtk_progress_bar_set_fraction ( level->bar_snr, (double)snr / 100 );

	char texta[20], textb[20];
	sprintf ( texta, "Sgn %u%%", sgl );
	sprintf ( textb, "Snr %u%%", snr );

	const char *text_l = "bfbfbf";
	if ( lock  ) text_l = "00ff00"; else text_l = "ff0000";
	if ( sgl == 0 && snr == 0 ) text_l = "bfbfbf";

	const char *rec_cl = ( level->pulse ) ? "ff0000" :"2b2222";

	time_t t_cur;
	time ( &t_cur );

	if ( ( t_cur > level->t_start ) ) { time ( &level->t_start ); level->pulse = !level->pulse; }

	g_autofree char *markup = g_markup_printf_escaped ( "%s<span foreground=\"#%s\">  ◉  </span>%s  <span foreground=\"#%s\">  %s</span>", 
		texta, text_l, textb, rec_cl, ( rec ) ? " ◉ " : "" );

	gtk_label_set_markup ( level->sgn_snr, markup );
}

static void level_init ( Level *level )
{
	GtkBox *box = GTK_BOX ( level );
	gtk_orientable_set_orientation ( GTK_ORIENTABLE ( box ), GTK_ORIENTATION_VERTICAL );
	gtk_box_set_spacing ( box, 3 );

	level->sgn_snr = (GtkLabel *)gtk_label_new ( "Signal  ◉  Snr" );
	level->bar_sgn = (GtkProgressBar *)gtk_progress_bar_new ();
	level->bar_snr = (GtkProgressBar *)gtk_progress_bar_new ();

	gtk_box_pack_start ( box, GTK_WIDGET ( level->sgn_snr ), FALSE, FALSE, 0 );
	gtk_box_pack_start ( box, GTK_WIDGET ( level->bar_sgn ), FALSE, FALSE, 0 );
	gtk_box_pack_start ( box, GTK_WIDGET ( level->bar_snr ), FALSE, FALSE, 0 );
}

static void level_finalize ( GObject *object )
{
	G_OBJECT_CLASS (level_parent_class)->finalize (object);
}

static void level_class_init ( LevelClass *class )
{
	G_OBJECT_CLASS (class)->finalize = level_finalize;
}

Level * level_new ( void )
{
	return g_object_new ( LEVEL_TYPE_BOX, NULL );
}
