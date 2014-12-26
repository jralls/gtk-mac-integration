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

#ifndef __GTKOSX_APPLICATIONPRIVATE_H__
#define __GTKOSX_APPLICATIONPRIVATE_H__
#import <AppKit/NSMenu.h>
#import <AppKit/NSApplication.h>

#include "gtkosxapplication.h"
#import "GtkApplicationNotify.h"
#import "GtkApplicationDelegate.h"

#define  GTKOSX_APPLICATION_GET_PRIVATE(obj)	(G_TYPE_INSTANCE_GET_PRIVATE ((obj), GTKOSX_TYPE_APPLICATION, GtkosxApplicationPrivate))

struct _GtkosxApplicationPrivate
{
  gboolean use_quartz_accelerators;
  gboolean listen_for_multimedia_keys;
  NSMenu *dock_menu;
  GtkApplicationNotificationObject *notify;
  GtkApplicationDelegate *delegate;
};

#endif
