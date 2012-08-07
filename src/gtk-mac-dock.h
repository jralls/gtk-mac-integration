/* GTK+ Integration for the Mac OS X Dock.
 *
 * Copyright (C) 2007, 2008 Imendio AB
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

#ifndef __GTK_MAC_DOCK_H__
#define __GTK_MAC_DOCK_H__

#include <gtk/gtk.h>
#include <gtk-mac-bundle.h>

G_BEGIN_DECLS

#define GTK_TYPE_MAC_DOCK            (gtk_mac_dock_get_type ())
#define GTK_MAC_DOCK(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_MAC_DOCK, GtkMacDock))
#define GTK_MAC_DOCK_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_MAC_DOCK, GtkMacDockClass))
#define GTK_IS_MAC_DOCK(obj)	     (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_MAC_DOCK))
#define GTK_IS_MAC_DOCK_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_MAC_DOCK))
#define GTK_MAC_DOCK_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_MAC_DOCK, GtkMacDockClass))

typedef struct _GtkMacDock      GtkMacDock;
typedef struct _GtkMacDockClass GtkMacDockClass;

typedef struct _GtkMacAttentionRequest GtkMacAttentionRequest;

struct _GtkMacDock
{
  GObject parent_instance;
};

struct _GtkMacDockClass
{
  GObjectClass parent_class;
};

typedef enum {
        GTK_MAC_ATTENTION_CRITICAL,
        GTK_MAC_ATTENTION_INFO
} GtkMacAttentionType;

GType                   gtk_mac_dock_get_type                  (void);
GtkMacDock *            gtk_mac_dock_new                       (void);
GtkMacDock *            gtk_mac_dock_get_default               (void);
void                    gtk_mac_dock_set_icon_from_pixbuf      (GtkMacDock             *dock,
                                                                GdkPixbuf              *pixbuf);
void                    gtk_mac_dock_set_icon_from_resource    (GtkMacDock             *dock,
                                                                GtkMacBundle           *bundle,
                                                                const gchar            *name,
                                                                const gchar            *type,
                                                                const gchar            *subdir);
void                    gtk_mac_dock_set_overlay_from_pixbuf   (GtkMacDock             *dock,
                                                                GdkPixbuf              *pixbuf);
void                    gtk_mac_dock_set_overlay_from_resource (GtkMacDock             *dock,
                                                                GtkMacBundle           *bundle,
                                                                const gchar            *name,
                                                                const gchar            *type,
                                                                const gchar            *subdir);
GtkMacAttentionRequest *gtk_mac_dock_attention_request         (GtkMacDock             *dock,
                                                                GtkMacAttentionType     type);
void                    gtk_mac_dock_attention_cancel          (GtkMacDock             *dock,
                                                                GtkMacAttentionRequest *request);

#define GTK_TYPE_MAC_ATTENTION_TYPE (gtk_mac_attention_type_get_type())
GType                   gtk_mac_attention_type_get_type        (void);

G_END_DECLS

#endif /* __GTK_MAC_DOCK_H__ */
