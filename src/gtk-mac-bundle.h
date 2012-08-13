/* GTK+ Integration for app bundles.
 *
 * Copyright (C) 2007-2008 Imendio AB
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

#ifndef __GTK_MAC_BUNDLE_H__
#define __GTK_MAC_BUNDLE_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define GTK_TYPE_MAC_BUNDLE            (gtk_mac_bundle_get_type ())
#define GTK_MAC_BUNDLE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_MAC_BUNDLE, GtkMacBundle))
#define GTK_MAC_BUNDLE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_MAC_BUNDLE, GtkMacBundleClass))
#define GTK_IS_MAC_BUNDLE(obj)	       (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_MAC_BUNDLE))
#define GTK_IS_MAC_BUNDLE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_MAC_BUNDLE))
#define GTK_MAC_BUNDLE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_MAC_BUNDLE, GtkMacBundleClass))

typedef struct _GtkMacBundle      GtkMacBundle;
typedef struct _GtkMacBundleClass GtkMacBundleClass;

struct _GtkMacBundle {
  GObject parent_instance;
};

struct _GtkMacBundleClass {
  GObjectClass parent_class;
};

GType         gtk_mac_bundle_get_type          (void);
GtkMacBundle *gtk_mac_bundle_new               (void);
GtkMacBundle *gtk_mac_bundle_get_default       (void);
void          gtk_mac_bundle_setup_environment (GtkMacBundle *bundle);
const gchar * gtk_mac_bundle_get_id            (GtkMacBundle *bundle);
const gchar * gtk_mac_bundle_get_path          (GtkMacBundle *bundle);
gboolean      gtk_mac_bundle_get_is_app_bundle (GtkMacBundle *bundle);
const gchar * gtk_mac_bundle_get_localedir     (GtkMacBundle *bundle);
const gchar * gtk_mac_bundle_get_datadir       (GtkMacBundle *bundle);
const gchar * gtk_mac_bundle_get_resourcesdir  (GtkMacBundle *bundle);
gchar *       gtk_mac_bundle_get_resource_path (GtkMacBundle *bundle,
                                                const gchar  *name,
                                                const gchar  *type,
                                                const gchar  *subdir);

G_END_DECLS

#endif /* __GTK_MAC_BUNDLE_H__ */
