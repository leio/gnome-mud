/* AMCL - A simple Mud CLient
 * Copyright (C) 1999 Robin Ericsson <lobbin@localhost.nu>
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

#include "config.h"
#include <gtk/gtk.h>

#include "amcl.h"

static char const rcsid[] = "$Id$";

void plugin_popup_message (gchar *message)
{
  popup_window (message);
}

void plugin_add_connection_text(CONNECTION_DATA *connection, gchar *message, gint color)
{
  if (connection == NULL || connection->window == NULL)
    textfield_add (main_connection->window, message, color);
  else
    textfield_add (connection->window, message, color);
}

gboolean plugin_register_menu (gint handle, gchar *name, gchar *function)
{
  GtkSignalFunc  sig_function;
  GtkWidget     *menu_place;

  if ((sig_function = (GtkSignalFunc) dlsym (handle, function)) == NULL) {
    g_message ("Error register menu: %s", dlerror());
    return FALSE;
  }
  
  menu_place = gtk_menu_item_new_with_label(name);
  gtk_menu_prepend (GTK_MENU (menu_plugin_menu), menu_place);
  gtk_widget_show (menu_place);
  gtk_signal_connect (GTK_OBJECT (menu_place), "activate",
		      sig_function, NULL);
  
  return TRUE;
}
