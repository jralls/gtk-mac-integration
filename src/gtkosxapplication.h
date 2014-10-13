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
#define GTKOSX_TYPE_APPLICATION	(gtkosx_application_get_type())
#define GTKOSX_APPLICATION(obj) 	(G_TYPE_CHECK_INSTANCE_CAST ((obj), GTKOSX_TYPE_APPLICATION, GtkosxApplication))
#define GTKOSX_IS_APPLICATION(obj)	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTKOSX_TYPE_APPLICATION))
#define GTKOSX_APPLICATION_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass),  GTKOSX_TYPE_APPLICATION, GtkosxApplicationClass))
#define GTKOSX_IS_APPLICATION_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass),  GTKOSX_TYPE_APPLICATION))
#define GTKOSX_APPLICATION_GET_CLASS(obj) 	(G_TYPE_INSTANCE_GET_CLASS ((obj), GTKOSX_TYPE_APPLICATION, GtkosxApplicationClass))

typedef struct _GtkosxApplication GtkosxApplication;
typedef struct _GtkosxApplicationPrivate GtkosxApplicationPrivate;
typedef struct _GtkosxApplicationClass GtkosxApplicationClass;
typedef struct _GtkosxApplicationMenuGroup GtkosxApplicationMenuGroup;

struct _GtkosxApplication
{
  GObject parent_instance;
  /*< private >*/
  GtkosxApplicationPrivate *priv;
};

struct _GtkosxApplicationClass
{
  GObjectClass parent_class;
};

struct _GtkosxApplicationMenuGroup
{
  GList *items;
};


GType gtkosx_application_get_type (void);
GtkosxApplication *gtkosx_application_get (void);

//void gtkosx_application_init (GtkosxApplication *self);
void gtkosx_application_ready (GtkosxApplication *self);

/*Accelerator functions*/

void gtkosx_application_set_use_quartz_accelerators(GtkosxApplication *self, 
					 gboolean use_quartz_accelerators);
gboolean gtkosx_application_use_quartz_accelerators(GtkosxApplication *self);

/*Menu functions*/
void gtkosx_application_set_menu_bar (GtkosxApplication *self, 
				      GtkMenuShell *menu_shell);
void gtkosx_application_sync_menubar (GtkosxApplication *self);

void gtkosx_application_insert_app_menu_item (GtkosxApplication *self,
					      GtkWidget *menu_item,
					      gint index);
void gtkosx_application_set_window_menu (GtkosxApplication *self,
					 GtkMenuItem *menu_item);
void gtkosx_application_set_help_menu (GtkosxApplication *self,
				       GtkMenuItem *menu_item);

/*Dock Functions*/

typedef enum {
  CRITICAL_REQUEST = 0,
  INFO_REQUEST = 10
} GtkosxApplicationAttentionType;

/*To satisfy h2defs.py */
#define GTKOSX_TYPE_APPLICATION_ATTENTION_TYPE	(gtkosx_type_application_attention_type_get_type())
// GType gtkosx_type_application_attention_type_get_type(void);

void gtkosx_application_set_dock_menu(GtkosxApplication *self, 
				   GtkMenuShell *menu_shell);
void gtkosx_application_set_dock_icon_pixbuf(GtkosxApplication *self,
					  GdkPixbuf *pixbuf);
void gtkosx_application_set_dock_icon_resource(GtkosxApplication *self,
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
gint gtkosx_application_attention_request(GtkosxApplication *self,
				       GtkosxApplicationAttentionType type);
void gtkosx_application_cancel_attention_request(GtkosxApplication *self, gint id);

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
 * g_object_new(GTKOSX_TYPE_APPLICATION) just to get it. */

gchar *gtkosx_application_get_bundle_path(void);
gchar *gtkosx_application_get_resource_path(void);
gchar *gtkosx_application_get_executable_path(void);
gchar *gtkosx_application_get_bundle_id(void);
gchar *gtkosx_application_get_bundle_info(const gchar *key);

/* Multimedia functions */
void gtkosx_application_set_listen_for_multimedia_keys(GtkosxApplication *self,
					 gboolean listen_for_multimedia_keys);
gboolean
gtkosx_application_listen_for_multimedia_keys (GtkosxApplication *self);

G_END_DECLS

#endif /* __GTK_OSX_APPLICATION_H__ */
