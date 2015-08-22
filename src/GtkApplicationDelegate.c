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

#import "GtkApplicationDelegate.h"
#include <gtk/gtk.h>
#include "gtkosxapplication.h"

static gchar *utf8_path = NULL;

@implementation GtkApplicationDelegate

-(void) applicationDidFinishLaunching: (NSNotification*)aNotification
{
  if (utf8_path != NULL)
    {
      GtkosxApplication *app = g_object_new (GTKOSX_TYPE_APPLICATION, NULL);
      guint sig = g_signal_lookup ("NSApplicationOpenFile",
				   GTKOSX_TYPE_APPLICATION);
      gboolean result = FALSE;
      if (sig)
	g_signal_emit (app, sig, 0, utf8_path, &result);
      g_object_unref (app);
    }
}

-(BOOL) application: (NSApplication*)theApplication openFile: (NSString*) file
{
  utf8_path =  [file UTF8String];
  GtkosxApplication *app = g_object_new (GTKOSX_TYPE_APPLICATION, NULL);
  guint sig = g_signal_lookup ("NSApplicationOpenFile",
			       GTKOSX_TYPE_APPLICATION);
  gboolean result = FALSE;
  if (sig)
    {
      g_signal_emit (app, sig, 0, utf8_path, &result);
      utf8_path = NULL;
    }
  g_object_unref (app);
  return result;
}


-(NSApplicationTerminateReply) applicationShouldTerminate: (NSApplication *)sender
{
  GtkosxApplication *app = g_object_new (GTKOSX_TYPE_APPLICATION, NULL);
  guint sig = g_signal_lookup ("NSApplicationBlockTermination",
  GTKOSX_TYPE_APPLICATION);
  gboolean result = FALSE;
  static gboolean inHandler = FALSE;
  if (inHandler) return NSTerminateCancel;
  if (sig)
    {
      inHandler = TRUE;
      g_signal_emit (app, sig, 0, &result);
    }

  g_object_unref (app);
  inHandler = FALSE;
  if (!result)
    {
      g_object_unref (app);
      return NSTerminateNow;
    }
  else
    return NSTerminateCancel;
}

extern NSMenu* _gtkosx_application_dock_menu (GtkosxApplication* app);

-(NSMenu *) applicationDockMenu: (NSApplication*)sender
{
  GtkosxApplication *app = g_object_new (GTKOSX_TYPE_APPLICATION, NULL);
  return _gtkosx_application_dock_menu (app);
}

-(void) getUrl:(NSAppleEventDescriptor *)event withReplyEvent:(NSAppleEventDescriptor *)replyEvent
{
  NSString *url = [[event paramDescriptorForKeyword:keyDirectObject] stringValue];
  GtkosxApplication *app = g_object_new(GTKOSX_TYPE_APPLICATION, NULL);
  guint sig = g_signal_lookup("NSApplicationOpenURL", GTKOSX_TYPE_APPLICATION);
  if (sig)
    g_signal_emit(app, sig, 0, [url UTF8String]);
  g_object_unref(app);
}

-(void)applicationWillFinishLaunching:(NSNotification *)aNotification {
  [[NSAppleEventManager sharedAppleEventManager]
	setEventHandler:self
	andSelector:@selector(getUrl:withReplyEvent:)
	forEventClass:kInternetEventClass
	andEventID:kAEGetURL];
}

@end
