/* GTK+ Integration for the Mac OS X Menubar.
 *
 * Copyright (C) 2007 Pioneer Research Center USA, Inc.
 * Copyright (C) 2007, 2008 Imendio AB
 * Copyright © 2009, 2010 John Ralls
 *
 * For further information, see:
 * http://sourceforge.net/apps/trac/gtk-osx/wiki/Integrate
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GTK_MAC_MENU_H__
#define __GTK_MAC_MENU_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct _GtkMacMenuGroup GtkMacMenuGroup;

void gtk_mac_menu_set_menu_bar (GtkMenuShell *menu_shell);
void gtk_mac_menu_set_quit_menu_item (GtkMenuItem *menu_item);
GtkMacMenuGroup *gtk_mac_menu_add_app_menu_group (void);
void gtk_mac_menu_add_app_menu_item (GtkMacMenuGroup *group, 
				     GtkMenuItem *menu_item, 
				     const gchar *label);
void gtk_mac_menu_sync (GtkMenuShell *menu_shell);
gboolean gtk_mac_menu_handle_menu_event (GdkEventKey *event);
void gtk_mac_menu_set_global_key_handler_enabled (gboolean enabled);
void gtk_mac_menu_connect_window_key_handler (GtkWindow *window);

G_END_DECLS

#endif /* __GTK_MAC_MENU_H__ */
