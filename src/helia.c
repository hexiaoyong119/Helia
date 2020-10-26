/*
* Copyright 2020 Stepan Perun
* This program is free software.
*
* License: Gnu General Public License GPL-3
* file:///usr/share/common-licenses/GPL-3
* http://www.gnu.org/licenses/gpl-3.0.html
*/

#include "default.h"
#include "dvb.h"
#include "file.h"
#include "scan.h"
#include "player.h"
#include "button.h"
#include "treeview.h"
#include "settings.h"

#include <gtk/gtk.h>
#include <gst/gst.h>

enum prefs
{
	PREF_RECORD,
	PREF_THEME,
	PREF_OPACITY,
	PREF_OPACITY_WIN,
	PREF_ICON_SIZE
};

#define HELIA_TYPE_APPLICATION                          helia_get_type ()

G_DECLARE_FINAL_TYPE ( Helia, helia, HELIA, APPLICATION, GtkApplication )

struct _Helia
{
	GtkApplication  parent_instance;

	GtkWindow *window;
	GtkPopover *popover;
	GSettings *setting;

	GtkBox *bs_vbox;
	GtkBox *mn_vbox;
	GtkBox *mp_vbox;
	GtkBox *tv_vbox;

	Dvb *dvb;
	Player *player;

	gboolean first_mp;
	gboolean first_tv;

	int width;
	int height;

	uint cookie;
	GDBusConnection *connect;
};

G_DEFINE_TYPE ( Helia, helia, GTK_TYPE_APPLICATION )

static uint helia_power_manager_inhibit ( GDBusConnection *connect )
{
	uint cookie;
	GError *err = NULL;

	GVariant *reply = g_dbus_connection_call_sync ( connect,
						"org.freedesktop.PowerManagement",
						"/org/freedesktop/PowerManagement/Inhibit",
						"org.freedesktop.PowerManagement.Inhibit",
						"Inhibit",
						g_variant_new ("(ss)", "Helia-light", "Video" ),
						G_VARIANT_TYPE ("(u)"),
						G_DBUS_CALL_FLAGS_NONE,
						-1,
						NULL,
						&err );

	if ( reply != NULL )
	{
		g_variant_get ( reply, "(u)", &cookie, NULL );
		g_variant_unref ( reply );

		return cookie;
	}

	if ( err )
	{
		g_warning ( "Inhibiting failed %s", err->message );
		g_error_free ( err );
	}

	return 0;
}

static void helia_power_manager_uninhibit ( GDBusConnection *connect, uint cookie )
{
	GError *err = NULL;

	GVariant *reply = g_dbus_connection_call_sync ( connect,
						"org.freedesktop.PowerManagement",
						"/org/freedesktop/PowerManagement/Inhibit",
						"org.freedesktop.PowerManagement.Inhibit",
						"UnInhibit",
						g_variant_new ("(u)", cookie),
						NULL,
						G_DBUS_CALL_FLAGS_NONE,
						-1,
						NULL,
						&err );

	if ( err )
	{
		g_warning ( "Uninhibiting failed %s", err->message );
		g_error_free ( err );
	}

	g_variant_unref ( reply );
}

static void helia_power_manager_on ( Helia *helia )
{
	if ( helia->cookie > 0 ) helia_power_manager_uninhibit ( helia->connect, helia->cookie );

	/*if ( helia->cookie > 0 ) gtk_application_uninhibit ( GTK_APPLICATION (helia), helia->cookie );*/

	helia->cookie = 0;
}

static void helia_power_manager_off ( Helia *helia )
{
	helia->cookie = helia_power_manager_inhibit ( helia->connect );

	/*helia->cookie = gtk_application_inhibit ( GTK_APPLICATION (helia), helia->window, GTK_APPLICATION_INHIBIT_SWITCH, "Helia-light" );*/
}

static void helia_power_manager ( gboolean power_off, Helia *helia )
{
	if ( helia->connect )
	{
		if ( power_off )
			helia_power_manager_off ( helia );
		else
			helia_power_manager_on  ( helia );
	}
}

static GDBusConnection * helia_dbus_init ( void )
{
	GError *err = NULL;

	GDBusConnection *connect = g_bus_get_sync ( G_BUS_TYPE_SESSION, NULL, &err );

	if ( err )
	{
		g_warning ( "%s:: Failed to get session bus: %s", __func__, err->message );
		g_error_free ( err );
	}

	return connect;
}

static void helia_about ( G_GNUC_UNUSED GtkButton *button, GtkWindow *window )
{
	GtkAboutDialog *dialog = (GtkAboutDialog *)gtk_about_dialog_new ();
	gtk_window_set_transient_for ( GTK_WINDOW ( dialog ), window );

	gtk_window_set_icon_name ( window, "applications-multimedia" );

	const char *authors[] = { "Stepan Perun", " ", NULL };

	gtk_about_dialog_set_program_name ( dialog, "Helia-light" );
	gtk_about_dialog_set_version ( dialog, "20.10" );
	gtk_about_dialog_set_authors ( dialog, authors );
	gtk_about_dialog_set_website ( dialog,   "https://github.com/vl-nix/helia" );
	gtk_about_dialog_set_copyright ( dialog, "Copyright 2020 Helia-light" );
	gtk_about_dialog_set_comments  ( dialog, "Media Player & IPTV & Digital TV \nDVB-T2/S2/C" );
	gtk_about_dialog_set_license_type ( dialog, GTK_LICENSE_GPL_3_0 );
	gtk_about_dialog_set_logo_icon_name ( dialog, "applications-multimedia" );

	gtk_dialog_run ( GTK_DIALOG (dialog) );
	gtk_widget_destroy ( GTK_WIDGET (dialog) );
}

static void helia_win_base ( Helia *helia )
{
	gtk_widget_hide ( GTK_WIDGET ( helia->mp_vbox ) );
	gtk_widget_hide ( GTK_WIDGET ( helia->tv_vbox ) );

	gtk_widget_show ( GTK_WIDGET ( helia->bs_vbox ) );

	dvb_run_status ( OPACITY, FALSE, helia->dvb );
	player_run_status ( OPACITY, FALSE, helia->player );

	gtk_window_set_title ( helia->window, "Helia-light" );
}

static void helia_window_set_win_mp ( G_GNUC_UNUSED GtkButton *button, Helia *helia )
{
	gtk_widget_hide ( GTK_WIDGET ( helia->bs_vbox ) );

	if ( helia->first_mp )
	{
		gtk_box_pack_start ( helia->mn_vbox, GTK_WIDGET ( helia->mp_vbox ), TRUE, TRUE, 0 );
		gtk_widget_show_all ( GTK_WIDGET ( helia->mp_vbox ) );
		helia->first_mp = FALSE;
	}
	else
		gtk_widget_show ( GTK_WIDGET ( helia->mp_vbox ) );

	uint16_t opacity = OPACITY;
	if ( helia->setting ) opacity = (uint16_t)g_settings_get_uint ( helia->setting, "opacity-panel" );

	player_run_status ( opacity, TRUE, helia->player );

	gtk_window_set_title ( helia->window, "Helia-light - Media Player");
}

static void helia_window_set_win_tv ( G_GNUC_UNUSED GtkButton *button, Helia *helia )
{
	gtk_widget_hide ( GTK_WIDGET ( helia->bs_vbox ) );

	if ( helia->first_tv )
	{
		gtk_box_pack_start ( helia->mn_vbox, GTK_WIDGET ( helia->tv_vbox ), TRUE, TRUE, 0 );
		gtk_widget_show_all ( GTK_WIDGET ( helia->tv_vbox ) );
		helia->first_tv = FALSE;
	}
	else
		gtk_widget_show ( GTK_WIDGET ( helia->tv_vbox ) );

	uint16_t opacity = OPACITY;
	if ( helia->setting ) opacity = (uint16_t)g_settings_get_uint ( helia->setting, "opacity-panel" );

	dvb_run_status ( opacity, TRUE, helia->dvb );

	gtk_window_set_title ( helia->window, "Helia-light - Digital TV" );
}

static void helia_dvb_clicked_handler ( G_GNUC_UNUSED Dvb *dvb, G_GNUC_UNUSED const char *button, Helia *helia )
{
	helia_win_base ( helia );
}

static void helia_player_clicked_handler ( G_GNUC_UNUSED Player *player, G_GNUC_UNUSED const char *button, Helia *helia )
{
	helia_win_base ( helia );
}

static void helia_dvb_power_handler ( G_GNUC_UNUSED Dvb *dvb, gboolean power, Helia *helia )
{
	helia_power_manager ( power, helia );
}

static void helia_player_power_handler ( G_GNUC_UNUSED Player *player, gboolean power, Helia *helia )
{
	helia_power_manager ( power, helia );
}

static void helia_button_changed_record ( GtkFileChooserButton *button, Helia *helia )
{
	g_autofree char *path = gtk_file_chooser_get_filename ( GTK_FILE_CHOOSER ( button ) );

	if ( path == NULL ) return;

	if ( helia->setting ) g_settings_set_string ( helia->setting, "rec-dir", path );

	gtk_widget_set_visible ( GTK_WIDGET ( helia->popover ), FALSE );
}

static void helia_button_changed_theme ( GtkFileChooserButton *button, Helia *helia )
{
	g_autofree char *path = gtk_file_chooser_get_filename ( GTK_FILE_CHOOSER ( button ) );

	if ( path == NULL ) return;

	g_autofree char *name = g_path_get_basename ( path );

	g_object_set ( gtk_settings_get_default (), "gtk-theme-name", name, NULL );

	if ( helia->setting ) g_settings_set_string ( helia->setting, "theme", name );

	gtk_widget_set_visible ( GTK_WIDGET ( helia->popover ), FALSE );
}

static GtkFileChooserButton * helia_header_bar_create_chooser_button ( const char *path, const char *text_i, enum prefs prf, Helia *helia )
{
	GtkFileChooserButton *button = (GtkFileChooserButton *)gtk_file_chooser_button_new ( text_i, GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER );
	gtk_file_chooser_set_current_folder ( GTK_FILE_CHOOSER ( button ), path );
	gtk_widget_set_tooltip_text ( GTK_WIDGET ( button ), text_i );

	if ( prf == PREF_RECORD ) g_signal_connect ( button, "file-set", G_CALLBACK ( helia_button_changed_record ), helia );
	if ( prf == PREF_THEME  ) g_signal_connect ( button, "file-set", G_CALLBACK ( helia_button_changed_theme  ), helia );

	gtk_widget_show ( GTK_WIDGET ( button ) );

	return button;
}

static void helia_spinbutton_changed_opacity ( GtkSpinButton *button, Helia *helia )
{
	uint opacity = (uint)gtk_spin_button_get_value_as_int ( button );

	if ( helia->setting ) g_settings_set_uint ( helia->setting, "opacity-panel", opacity );
}

static void helia_spinbutton_changed_opacity_win ( GtkSpinButton *button, Helia *helia )
{
	uint opacity = (uint)gtk_spin_button_get_value_as_int ( button );

	gtk_widget_set_opacity ( GTK_WIDGET ( helia->window ), ( (float)opacity / 100) );

	if ( helia->setting ) g_settings_set_uint ( helia->setting, "opacity-win", opacity );
}
/*
static void helia_spinbutton_changed_size_i ( GtkSpinButton *button, Helia *helia )
{
	uint icon_size = (uint)gtk_spin_button_get_value_as_int ( button );

	if ( helia->setting ) g_settings_set_uint ( helia->setting, "icon-size", icon_size );
}
*/
static void helia_clicked_open_f ( G_GNUC_UNUSED GtkButton *button, Helia *helia )
{
	gtk_widget_set_visible ( GTK_WIDGET ( helia->popover ), FALSE );

	GSList *files = helia_open_files ( g_get_home_dir (), helia->window );

	if ( files == NULL ) return;

	while ( files != NULL )
	{
		helia_add_file ( files->data, helia->player );
		files = files->next;
	}

	g_slist_free_full ( files, (GDestroyNotify) g_free );

	helia_window_set_win_mp ( NULL, helia );
}

static void helia_clicked_open_d ( G_GNUC_UNUSED GtkButton *button, Helia *helia )
{
	gtk_widget_set_visible ( GTK_WIDGET ( helia->popover ), FALSE );

	g_autofree char *path = helia_open_dir ( g_get_home_dir (), helia->window );

	if ( path == NULL ) return;

	helia_add_dir ( path, helia->player );

	helia_window_set_win_mp ( NULL, helia );
}

static void helia_clicked_open_n ( G_GNUC_UNUSED GtkButton *button, Helia *helia )
{
	gtk_widget_set_visible ( GTK_WIDGET ( helia->popover ), FALSE );

	helia_open_net ( helia->window, helia->player );

	helia_window_set_win_mp ( NULL, helia );
}

static void helia_clicked_dark ( G_GNUC_UNUSED GtkButton *button, Helia *helia )
{
	gboolean dark = TRUE;
	g_object_get ( gtk_settings_get_default(), "gtk-application-prefer-dark-theme", &dark, NULL );
	g_object_set ( gtk_settings_get_default(), "gtk-application-prefer-dark-theme", !dark, NULL );

	if ( helia->setting ) g_settings_set_boolean ( helia->setting, "dark", !dark );
}

static void helia_clicked_info ( G_GNUC_UNUSED GtkButton *button, Helia *helia )
{
	gtk_widget_set_visible ( GTK_WIDGET ( helia->popover ), FALSE );

	helia_about ( NULL, helia->window );
}

static void helia_clicked_quit ( G_GNUC_UNUSED GtkButton *button, Helia *helia )
{
	gtk_widget_destroy ( GTK_WIDGET ( helia->window ) );
}

static GtkSpinButton * helia_header_bar_create_spinbutton ( uint val, uint16_t min, uint16_t max, uint16_t step, const char *text, enum prefs prf, Helia *helia )
{
	GtkSpinButton *spinbutton = (GtkSpinButton *)gtk_spin_button_new_with_range ( min, max, step );
	gtk_spin_button_set_value ( spinbutton, val );

	const char *icon = helia_check_icon_theme ( "helia-info" ) ? "helia-info" : "info";
	gtk_entry_set_icon_from_icon_name ( GTK_ENTRY ( spinbutton ), GTK_ENTRY_ICON_PRIMARY, icon );
	gtk_entry_set_icon_tooltip_text   ( GTK_ENTRY ( spinbutton ), GTK_ENTRY_ICON_PRIMARY, text );

	if ( prf == PREF_OPACITY     ) g_signal_connect ( spinbutton, "changed", G_CALLBACK ( helia_spinbutton_changed_opacity     ), helia );
	if ( prf == PREF_OPACITY_WIN ) g_signal_connect ( spinbutton, "changed", G_CALLBACK ( helia_spinbutton_changed_opacity_win ), helia );
	// if ( prf == PREF_ICON_SIZE   ) g_signal_connect ( spinbutton, "changed", G_CALLBACK ( helia_spinbutton_changed_size_i      ), helia );

	gtk_widget_show ( GTK_WIDGET ( spinbutton ) );

	return spinbutton;
}

static GtkMenuButton * helia_menu_button ( Helia *helia )
{
	GtkMenuButton *menu = (GtkMenuButton *)gtk_menu_button_new ();
	gtk_button_set_label ( GTK_BUTTON ( menu ), "𝋯" );

	helia->popover = (GtkPopover *)gtk_popover_new ( GTK_WIDGET ( NULL ) );
	gtk_popover_set_position ( helia->popover, GTK_POS_TOP );
	gtk_container_set_border_width ( GTK_CONTAINER ( helia->popover ), 5 );

	GtkBox *vbox = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_VERTICAL, 5 );
	gtk_box_set_spacing ( vbox, 5 );

	GtkBox *hbox = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 5 );
	gtk_box_set_spacing ( hbox, 5 );

	struct Data { const char *icon_u; const char *name; void ( *f )(GtkButton *, Helia *); };

	struct Data data_na[] =
	{
		{ "+",  "helia-add", helia_clicked_open_f },
		{ "🗁", "helia-dir", helia_clicked_open_d },
		{ "⇄",  "helia-net", helia_clicked_open_n }
	};

	uint8_t c = 0; for ( c = 0; c < G_N_ELEMENTS ( data_na ); c++ )
	{
		GtkButton *button = helia_create_button ( NULL, data_na[c].name, data_na[c].icon_u, ICON_SIZE );
		g_signal_connect ( button, "clicked", G_CALLBACK ( data_na[c].f ), helia );
		gtk_widget_show ( GTK_WIDGET ( button ) );
		gtk_box_pack_start ( hbox, GTK_WIDGET ( button ), TRUE, TRUE, 0 );
	}

	gtk_box_pack_start ( vbox, GTK_WIDGET ( hbox ), FALSE, FALSE, 0 );
	gtk_widget_show ( GTK_WIDGET ( hbox ) );

	uint16_t /*icon_size = ICON_SIZE,*/ opacity = OPACITY, opacity_win = 100;
	// if ( helia->setting ) icon_size   = (uint16_t)g_settings_get_uint ( helia->setting, "icon-size" );
	if ( helia->setting ) opacity_win = (uint16_t)g_settings_get_uint ( helia->setting, "opacity-win" );
	if ( helia->setting ) opacity     = (uint16_t)g_settings_get_uint ( helia->setting, "opacity-panel" );

	char path[PATH_MAX];

	g_autofree char *theme = NULL;
	g_autofree char *name  = NULL;

	g_object_get ( gtk_settings_get_default (), "gtk-theme-name", &name, NULL );
	if ( helia->setting ) theme = g_settings_get_string ( helia->setting, "theme" );

	if ( theme && !g_str_has_prefix ( theme, "none" ) )
		sprintf ( path, "/usr/share/themes/%s", theme );
	else
		sprintf ( path, "/usr/share/themes/%s", name );

	g_autofree char *rec_dir = NULL;
	if ( helia->setting ) rec_dir = g_settings_get_string ( helia->setting, "rec-dir" );
	if ( rec_dir && g_str_has_prefix ( rec_dir, "none" ) ) { free ( rec_dir ); rec_dir = NULL; }

	gtk_box_pack_start ( vbox, GTK_WIDGET ( helia_header_bar_create_spinbutton ( opacity,     40, 100, 1, "Opacity-Panel",  PREF_OPACITY,     helia ) ), FALSE, FALSE, 0 );
	gtk_box_pack_start ( vbox, GTK_WIDGET ( helia_header_bar_create_spinbutton ( opacity_win, 40, 100, 1, "Opacity-Window", PREF_OPACITY_WIN, helia ) ), FALSE, FALSE, 0 );
	// gtk_box_pack_start ( vbox, GTK_WIDGET ( helia_header_bar_create_spinbutton ( icon_size,    8,  48, 1, "Icon-size",      PREF_ICON_SIZE,   helia ) ), FALSE, FALSE, 0 );

	gtk_box_pack_start ( vbox, GTK_WIDGET ( helia_header_bar_create_chooser_button ( ( rec_dir ) ? rec_dir : g_get_home_dir (), "Record", PREF_RECORD, helia ) ), FALSE, FALSE, 0 );
	gtk_box_pack_start ( vbox, GTK_WIDGET ( helia_header_bar_create_chooser_button ( path, "Theme", PREF_THEME, helia ) ), FALSE, FALSE, 0 );

	struct Data data_n[] =
	{
		{ "⏾", "helia-dark",  helia_clicked_dark },
		{ "🛈", "helia-info",  helia_clicked_info },
		{ "⏻", "helia-quit",  helia_clicked_quit }
	};

	hbox = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 5 );
	gtk_box_set_spacing ( hbox, 5 );

	for ( c = 0; c < G_N_ELEMENTS ( data_n ); c++ )
	{
		GtkButton *button = helia_create_button ( NULL, data_n[c].name, data_n[c].icon_u, ICON_SIZE );
		g_signal_connect ( button, "clicked", G_CALLBACK ( data_n[c].f ), helia );
		gtk_widget_show ( GTK_WIDGET ( button ) );
		gtk_box_pack_start ( hbox, GTK_WIDGET ( button ), TRUE, TRUE, 0 );
	}

	gtk_box_pack_start ( vbox, GTK_WIDGET ( hbox ), FALSE, FALSE, 0 );
	gtk_widget_show ( GTK_WIDGET ( hbox ) );

	gtk_container_add ( GTK_CONTAINER ( helia->popover ), GTK_WIDGET ( vbox ) );
	gtk_widget_show ( GTK_WIDGET ( vbox ) );

	gtk_menu_button_set_popover ( menu, GTK_WIDGET ( helia->popover ) );

	return menu;
}

static void helia_window_quit ( G_GNUC_UNUSED GtkWindow *window, Helia *helia )
{
	dvb_quit ( helia->dvb );
	player_quit ( helia->player );

	if ( helia->setting ) g_object_unref ( helia->setting );
}

static void helia_set_settings ( Helia *helia )
{
	if ( !helia->setting ) return;

	uint opacity = 100;
	opacity = g_settings_get_uint ( helia->setting, "opacity-win" );
	gtk_widget_set_opacity ( GTK_WIDGET ( helia->window ), ( (float)opacity / 100) );

	gboolean dark = TRUE;
	dark = g_settings_get_boolean ( helia->setting, "dark" );
	g_object_set ( gtk_settings_get_default(), "gtk-application-prefer-dark-theme", dark, NULL );

	g_autofree char *theme = NULL;
	theme = g_settings_get_string ( helia->setting, "theme" );
	if ( theme && !g_str_has_prefix ( theme, "none" ) ) g_object_set ( gtk_settings_get_default (), "gtk-theme-name", theme, NULL );
}

static void helia_size_event ( GtkWindow *window, G_GNUC_UNUSED GdkEventConfigure *ev, Helia *helia )
{
	if ( !helia->setting ) return;

	int new_width = 0, new_height = 0;
	gtk_window_get_size ( window, &new_width, &new_height );

	if ( helia->width == new_width && helia->height == new_height ) return;

	helia->width  = new_width;
	helia->height = new_height;

	g_settings_set_uint ( helia->setting, "width",  (uint)new_width  );
	g_settings_set_uint ( helia->setting, "height", (uint)new_height );
}

static void helia_auto_start ( GFile **files, int n_files, Helia *helia )
{
	g_autofree char *fl_ch = g_file_get_basename ( files[0] );

	if ( g_str_has_prefix ( fl_ch, "channel" ) )
	{
		g_autofree char *ch = NULL;
		if ( n_files == 2 ) ch = g_file_get_basename ( files[1] );

		helia_window_set_win_tv ( NULL, helia );
		dvb_start_channel ( ch, helia->dvb );
	}
	else
	{
		helia_window_set_win_mp ( NULL, helia );
		helia_start_file ( files, n_files, helia->player );
	}
}

static void helia_window_new ( GApplication *app, GFile **files, int n_files )
{
	Helia *helia = HELIA_APPLICATION ( app );

	helia->connect = helia_dbus_init ();

	gtk_icon_theme_add_resource_path ( gtk_icon_theme_get_default (), "/helia" );

	if ( helia->setting ) helia->width  = (int)g_settings_get_uint ( helia->setting, "width"  );
	if ( helia->setting ) helia->height = (int)g_settings_get_uint ( helia->setting, "height" );

	helia->tv_vbox = GTK_BOX ( helia->dvb = dvb_new () );
	g_signal_connect ( helia->dvb, "power-set", G_CALLBACK ( helia_dvb_power_handler ), helia );
	g_signal_connect ( helia->dvb, "button-clicked", G_CALLBACK ( helia_dvb_clicked_handler ), helia );

	helia->mp_vbox = GTK_BOX ( helia->player = player_new () );
	g_signal_connect ( helia->player, "power-set", G_CALLBACK ( helia_player_power_handler ), helia );
	g_signal_connect ( helia->player, "button-clicked", G_CALLBACK ( helia_player_clicked_handler ), helia );

	helia->window = (GtkWindow *)gtk_application_window_new ( GTK_APPLICATION (helia) );
	gtk_window_set_title ( helia->window, "Helia-light");
	gtk_window_set_default_size ( helia->window, helia->width, helia->height );
	gtk_window_set_icon_name ( helia->window, "applications-multimedia" );
	g_signal_connect ( helia->window, "destroy", G_CALLBACK ( helia_window_quit ), helia );

	gtk_widget_set_events ( GTK_WIDGET ( helia->window ), GDK_STRUCTURE_MASK );
	g_signal_connect ( helia->window, "configure-event", G_CALLBACK ( helia_size_event ), helia );

	helia->mn_vbox = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_VERTICAL, 0 );

	helia->bs_vbox = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_VERTICAL, 0 );

	gtk_container_set_border_width ( GTK_CONTAINER ( helia->bs_vbox ), 25 );

	gtk_box_set_spacing ( helia->mn_vbox, 10 );
	gtk_box_set_spacing ( helia->bs_vbox, 10 );

	GtkBox *bt_hbox = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_box_set_spacing ( bt_hbox, 10 );

	gboolean icon_mp = helia_check_icon_theme ( "helia-mp" );

	GtkButton *button = helia_create_button ( bt_hbox, "helia-mp", "🎬", 96 );
	g_signal_connect ( button, "clicked", G_CALLBACK ( helia_window_set_win_mp ), helia );

	if ( !icon_mp )
	{
		GList *list = gtk_container_get_children ( GTK_CONTAINER (button) );
		gtk_label_set_markup ( GTK_LABEL (list->data), "<span font='48'>🎬</span>");
	}

	gboolean icon_tv = helia_check_icon_theme ( "helia-tv" );

	button = helia_create_button ( bt_hbox, "helia-tv", "🖵", 96 );
	g_signal_connect ( button, "clicked", G_CALLBACK ( helia_window_set_win_tv ), helia );

	if ( !icon_tv )
	{
		GList *list = gtk_container_get_children ( GTK_CONTAINER (button) );
		gtk_label_set_markup ( GTK_LABEL (list->data), "<span font='48'>🖵</span>");
	}

	gtk_box_pack_start ( helia->bs_vbox, GTK_WIDGET ( bt_hbox ), TRUE,  TRUE,  0 );

	GtkBox *bc_hbox = (GtkBox *)gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_box_set_spacing ( bc_hbox,  10 );

	GtkMenuButton *mb = helia_menu_button ( helia );
	gtk_box_pack_start ( bc_hbox, GTK_WIDGET ( mb ), TRUE,  TRUE,  0 );

	gtk_box_pack_start ( helia->bs_vbox, GTK_WIDGET ( bc_hbox ), FALSE, FALSE, 0 );
	gtk_box_pack_start ( helia->mn_vbox, GTK_WIDGET ( helia->bs_vbox ), TRUE,  TRUE,  0 );

	gtk_container_add   ( GTK_CONTAINER ( helia->window ), GTK_WIDGET ( helia->mn_vbox ) );

	helia_set_settings ( helia );
	gtk_widget_show_all ( GTK_WIDGET ( helia->window ) );

	player_add_accel ( GTK_APPLICATION ( app ), helia->player );

	if ( n_files ) helia_auto_start ( files, n_files, helia );
}

static void helia_activate ( GApplication *app )
{
	helia_window_new ( app, NULL, 0 );
}

static void helia_open ( GApplication *app, GFile **files, int n_files, G_GNUC_UNUSED const char *hint )
{
	helia_window_new ( app, files, n_files );
}

static void helia_init ( Helia *helia )
{
	helia->first_mp = TRUE;
	helia->first_tv = TRUE;

	helia->width  = 900;
	helia->height = 400;

	helia_dvb_init ( 0, 0 );

	helia->setting = settings_init ();
}

static void helia_finalize ( GObject *object )
{
	G_OBJECT_CLASS (helia_parent_class)->dispose (object);
}

static void helia_class_init ( HeliaClass *class )
{
	G_APPLICATION_CLASS (class)->activate = helia_activate;
	G_APPLICATION_CLASS (class)->open     = helia_open;

	G_OBJECT_CLASS (class)->finalize = helia_finalize;
}

static Helia * helia_new (void)
{
	return g_object_new ( HELIA_TYPE_APPLICATION, "flags", G_APPLICATION_HANDLES_OPEN, NULL );
}

int main ( int argc, char *argv[] )
{
	gst_init ( NULL, NULL );

	Helia *app = helia_new ();

	int status = g_application_run ( G_APPLICATION (app), argc, argv );

	g_object_unref (app);

	return status;
}
