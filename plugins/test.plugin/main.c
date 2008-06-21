/* GNOME-Mud - A simple Mud CLient
 * Copyright (C) 1998-2006 Robin Ericsson <lobbin@localhost.nu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#define __MODULE__
#include <gmodule.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <string.h>

#include "../../src/modules_api.h"
#include "../../config.h"

static void init_plugin   (PLUGIN_OBJECT *, GModule *);

PLUGIN_INFO gnomemud_plugin_info =
{
    "Test Plugin",
    "Robin Ericsson",
    "1.1",
    "Small plugin just to show how the plugins are supposed to work.",
    init_plugin,
};

void init_plugin(PLUGIN_OBJECT *plugin, GModule *context)
{
  plugin_popup_message ("Test Plugin Registered");
  plugin_register_menu(context, "Test Plugin", "menu_function");
  plugin_register_data_incoming(context, "data_in_function");
}

void data_in_function(PLUGIN_OBJECT *plugin, gchar *data, guint length, MudConnectionView *view)
{
  g_message("Received (%d) bytes.", length);
  plugin_add_connection_text("Plugin Called!", 0, view);
}

void menu_function(GtkWidget *widget, gint data)
{
    GtkWidget *label;
    GtkWidget *button;
    GtkWidget *main_box;
    GtkWidget *box2;
    GtkWidget *a_window;
    GtkWidget *separator;

    a_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title (GTK_WINDOW (a_window),"About, Olle Olle!!!");

    main_box = gtk_vbox_new (FALSE, 0);
    gtk_container_border_width (GTK_CONTAINER (main_box), 5);
    gtk_container_add (GTK_CONTAINER (a_window), main_box);

    label = gtk_label_new (PACKAGE_STRING);
    gtk_box_pack_start (GTK_BOX (main_box), label, FALSE, FALSE, 5);
    gtk_widget_show (label);

    label = gtk_label_new ("Copyright 1998-2006 Robin Ericsson <lobbin@localhost.nu>");
    gtk_box_pack_start (GTK_BOX (main_box), label, FALSE, FALSE, 0);
    gtk_widget_show (label);

    label = gtk_label_new ("Licensed under the terms of the GNU GENERAL PUBLIC LICENSE (GPL) version 2 or later.");
    gtk_box_pack_start (GTK_BOX (main_box), label, FALSE, FALSE, 5);
    gtk_widget_show (label);

    label = gtk_label_new ("Homepage: http://amcl.sourceforge.net/");
    gtk_box_pack_start (GTK_BOX (main_box), label, FALSE, FALSE, 5);
    gtk_widget_show (label);

    separator = gtk_hseparator_new ();
    gtk_box_pack_start (GTK_BOX (main_box), separator, FALSE, TRUE, 5);
    gtk_widget_show (separator);

    box2 = gtk_hbox_new (FALSE, 5);
    gtk_box_pack_start (GTK_BOX (main_box), box2, FALSE, FALSE, 0);

    button = gtk_button_new_with_label ( "Close");
    gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
                        GTK_SIGNAL_FUNC (gtk_widget_destroy), (gpointer) a_window);
    gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 5);

    gtk_widget_show (button  );
    gtk_widget_show (box2    );
    gtk_widget_show (main_box);
    gtk_widget_show (a_window);
};
