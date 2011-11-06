/* GTK+ Integration with platform-specific application-wide features 
 * such as the OS X menubar and application delegate concepts.
 *
 * Copyright (C) 2009 Paul Davis
 * Copyright © 2010 John Ralls
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; version 2.1
 * of the License.
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

#ifndef __GTK_OSX_APPLICATION_H__
#define __GTK_OSX_APPLICATION_H__

#include <gtk/gtk.h>
#include <glib-object.h>


G_BEGIN_DECLS
#define GTK_TYPE_OSX_APPLICATION	(gtk_osxapplication_get_type())
#define GTK_OSX_APPLICATION(obj) 	(G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_OSX_APPLICATION, GtkOSXApplication))
#define GTK_IS_OSX_APPLICATION(obj)	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_OSX_APPLICATION))
#define GTK_OSX_APPLICATION_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass),  GTK_TYPE_OSX_APPLICATION, GtkOSXApplicationClass))
#define GTK_IS_OSX_APPLICATION_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass),  GTK_TYPE_OSX_APPLICATION))
#define GTK_OSX_APPLICATION_GET_CLASS(obj) 	(G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_OSX_APPLICATION, GtkOSXApplicationClass))

typedef struct _GtkOSXApplication GtkOSXApplication;
typedef struct _GtkOSXApplicationPrivate GtkOSXApplicationPrivate;
typedef struct _GtkOSXApplicationClass GtkOSXApplicationClass;
typedef struct _GtkOSXApplicationMenuGroup GtkOSXApplicationMenuGroup;

struct _GtkOSXApplication
{
  GObject parent_instance;
  /*< private >*/
  GtkOSXApplicationPrivate *priv;
};

struct _GtkOSXApplicationClass
{
  GObjectClass parent_class;
  void (*should_load) (GtkOSXApplication *self, gchar *utf8_path);
};

struct _GtkOSXApplicationMenuGroup
{
  GList *items;
};


GType gtk_osxapplication_get_type (void);
//GtkOSXApplication *gtk_osxapplication_get (void);

//void gtk_osxapplication_init (GtkOSXApplication *self);
void gtk_osxapplication_ready (GtkOSXApplication *self);
void gtk_osxapplication_cleanup (GtkOSXApplication *self);

/*Accelerator functions*/

void gtk_osxapplication_set_use_quartz_accelerators(GtkOSXApplication *self, 
					 gboolean use_quartz_accelerators);
gboolean gtk_osxapplication_use_quartz_accelerators(GtkOSXApplication *self);

/*Menu functions*/
void gtk_osxapplication_set_menu_bar (GtkOSXApplication *self, 
				      GtkMenuShell *menu_shell);
void gtk_osxapplication_sync_menubar (GtkOSXApplication *self);

void gtk_osxapplication_insert_app_menu_item (GtkOSXApplication *self,
					      GtkWidget *menu_item,
					      gint index);
void gtk_osxapplication_set_window_menu (GtkOSXApplication *self,
					 GtkMenuItem *menu_item);
void gtk_osxapplication_set_help_menu (GtkOSXApplication *self,
				       GtkMenuItem *menu_item);

/*Dock Functions*/

typedef enum {
  CRITICAL_REQUEST = 0,
  INFO_REQUEST = 10
} GtkOSXApplicationAttentionType;

/*To satisfy h2defs.py */
#define GTK_TYPE_OSX_APPLICATION_ATTENTION_TYPE	(gtk_type_osxapplication_attention_type_get_type())
GType gtk_type_osxapplication_attention_type_get_type(void);

void gtk_osxapplication_set_dock_menu(GtkOSXApplication *self, 
				   GtkMenuShell *menu_shell);
void gtk_osxapplication_set_dock_icon_pixbuf(GtkOSXApplication *self,
					  GdkPixbuf *pixbuf);
void gtk_osxapplication_set_dock_icon_resource(GtkOSXApplication *self,
					    const gchar  *name,
					    const gchar  *type,
					    const gchar  *subdir);
/* Gtk-mac-dock provided two functions,
 * gtk_mac_dock_set_overlay_from_pixbuf and
 * gtk_mac_doc_set_overlay_from_resource, but OSX 10.5 and later do
 * not support application dock tile overlays. Document dock tiles
 * will by default represent a miniaturized view of the document's
 * contents badged with an even more miniaturized application
 * icon. The interface to change this is a bit complex and will be
 * left up to the application rather than implemented here.
 */
gint gtk_osxapplication_attention_request(GtkOSXApplication *self,
				       GtkOSXApplicationAttentionType type);
void gtk_osxapplication_cancel_attention_request(GtkOSXApplication *self, gint id);

/* Bundle Functions */
/* gtk-mac-bundle included a bunch of silly stuff for setting up the
 * environment. It's silly first because that's easier to do with a
 * startup script, and even easier to do with an LCEnvironment
 * dictionary in the bundle's Info.plist. 

 * Gtk applications, at least when launched with a shell script, still
 * return a bundle identifier and it's executable path is correct and
 * useful.  Gtk-mac-bundle had a "is it an application bundle"
 * function, but NSBundle doesn't provide that; instead,
 * quartz_application_get_bundle_id will return NULL if it's not really a
 * bundle, there's no Info.plist, or if Info.plist doesn't have a
 * CFBundleIdentifier key (So if you need to detect being in a bundle,
 * make sure that your bundle has that key!) 

 * Richard Proctor pointed out that these functions don't really need
 * to be class functions: the self parameter isn't used, and making
 * them "free" functions will often save one from having to call
 * g_object_new(GTK_TYPE_OSX_APPLICATION) just to get it. */

gchar *quartz_application_get_bundle_path(void);
gchar *quartz_application_get_resource_path(void);
gchar *quartz_application_get_executable_path(void);
gchar *quartz_application_get_bundle_id(void);
gchar *quartz_application_get_bundle_info(const gchar *key);

G_END_DECLS

#endif /* __GTK_OSX_APPLICATION_H__ */
