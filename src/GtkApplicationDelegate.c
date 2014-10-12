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


CGEventRef eventTapFunction (
   CGEventTapProxy proxy,
   CGEventType type,
   CGEventRef event,
   GtkApplicationDelegate *self
) {
	if(type == kCGEventTapDisabledByTimeout || type == kCGEventTapDisabledByUserInput)
	{
		printf(__FILE__": eventTapFunction disabled, calling reenable()\n");
		[self re_enable];
	}
	if(type < 0 || type > 0x7fffffff){
		printf(__FILE__": eventTapFunction invalid type: %u\n", type);
		return event;
	}
	NSEvent* nsevent = [NSEvent eventWithCGEvent:event];
	if( [nsevent type] == NSSystemDefined && [nsevent subtype] == 8 )
	{
		int keyCode = (([nsevent data1] & 0xFFFF0000) >> 16);
		int keyFlags = ([nsevent data1] & 0x0000FFFF);
		gboolean keyDown = (((keyFlags & 0xFF00) >> 8)) == 0xA;
		gboolean keyRepeat = (keyFlags & 0x1);

		printf(__FILE__ ": eventTapFunction mm(%i,%i,%i,%i)\n",
			keyCode,
			keyFlags,
			keyDown,
			keyRepeat);

		GtkosxApplication *app = g_object_new (GTKOSX_TYPE_APPLICATION, NULL);
		guint sig = g_signal_lookup ("MultiMediaKey",
			GTKOSX_TYPE_APPLICATION);
		gboolean result = FALSE;
		static gboolean inHandler = FALSE;
		if (inHandler) return event;
		if (sig)
		{
			inHandler = TRUE;
			g_signal_emit (app, sig, 0, keyCode, keyDown, keyRepeat, &result);
		}
		else
		{
			printf(__FILE__ ": signal not found\n");
		}

		g_object_unref (app);
		inHandler = FALSE;
		if (result)
			return NULL;
	}
	return event;
}

@implementation GtkApplicationDelegate
-(BOOL) application: (NSApplication*)theApplication openFile: (NSString*) file
{
  const gchar *utf8_path =  [file UTF8String];
  GtkosxApplication *app = g_object_new (GTKOSX_TYPE_APPLICATION, NULL);
  guint sig = g_signal_lookup ("NSApplicationOpenFile",
  GTKOSX_TYPE_APPLICATION);
  gboolean result = FALSE;
  if (sig)
    g_signal_emit (app, sig, 0, utf8_path, &result);
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
    return NSTerminateNow;
  else
    return NSTerminateCancel;
}

extern NSMenu* _gtkosx_application_dock_menu (GtkosxApplication* app);

-(NSMenu *) applicationDockMenu: (NSApplication*)sender
{
  GtkosxApplication *app = g_object_new (GTKOSX_TYPE_APPLICATION, NULL);
  return _gtkosx_application_dock_menu (app);
}


-(void)myThreadMainMethod:(id)object
{
	fprintf(stderr, "myThreadMainMethod started\n");
	// Top-level pool
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

	CFRunLoopRef runLoop = CFRunLoopGetCurrent();

	machPort = CGEventTapCreate(kCGSessionEventTap,
	                            kCGHeadInsertEventTap,
	                            kCGEventTapOptionDefault,
	                            CGEventMaskBit(NSSystemDefined),
	                            (CGEventTapCallBack)eventTapFunction,
	                            self);
	if (!machPort) {
		NSException *e =
		[NSException exceptionWithName:@"FailedToCreateEventTap"
			reason:@"Failed to create an event tap for multimedia keys"
			userInfo:[NSDictionary dictionary]];
		@throw e;
	}

	CFRunLoopSourceRef runLoopSource = CFMachPortCreateRunLoopSource(NULL,
	                                                                 machPort,
	                                                                 0);

	CFRunLoopAddSource(runLoop, runLoopSource, kCFRunLoopDefaultMode);
	CGEventTapEnable(machPort, true);


	do
	{
		[[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode
	                                beforeDate:[NSDate distantFuture]];
	}
	while (!shouldExit);

	fprintf(stderr, "myThreadMainMethod stopped\n");
	machPort = NULL;

	// Release the objects in the pool.
	[pool release];
}


- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
	fprintf(stderr, "applicationDidFinishLaunching\n");
	NSThread* myThread = [[NSThread alloc] initWithTarget:self
                                        selector:@selector(myThreadMainMethod:)
                                        object:nil];

	[myThread start];
}


- (void)applicationWillTerminate:(NSNotification *)aNotification
{
	fprintf(stderr, __FILE__": applicationWillTerminate\n");
	shouldExit = true;
}

@end
