/* GTK+ application-level integration for the Mac OS X/Cocoa
 *
 * Copyright (C) 2007 Pioneer Research Center USA, Inc.
 * Copyright (C) 2007 Imendio AB
 * Copyright (C) 2009 Paul Davis
 *
 * This is a reimplementation in Cocoa of the sync-menu.c concept
 * from Imendio, although without the "set quit menu" API since
 * a Cocoa app needs to handle termination anyway.
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

#include <gtk/gtk.h>

#import "GtkApplicationDelegate.h"
#import "GtkApplicationNotify.h"
#import "GNSMenuBar.h"
#import "GNSMenuItem.h"

#include <config.h>
#include "gtkosxapplication.h"
#include "gtkosxapplicationprivate.h"
#include "cocoa_menu_item.h"
#include "cocoa_menu.h"
#include "getlabel.h"
#include "gtkosx-image.h"

#ifdef G_LOG_DOMAIN
#undef G_LOG_DOMAIN
#endif
#define G_LOG_DOMAIN "gtkosxapplication"

/* This is a private function in libgdk; we need to have is so that we
   can force new windows onto the Window menu */
extern NSWindow* gdk_quartz_window_get_nswindow (GdkWindow*);

//#define DEBUG(format, ...) g_printerr ("%s: " format, G_STRFUNC, ## __VA_ARGS__)
#define DEBUG(format, ...)

/* TODO
 *
 * - Sync adding/removing/reordering items
 * - Create on demand? (can this be done with gtk+? ie fill in menu
 items when the menu is opened)
 * - Figure out what to do per app/window...
 *
 */
G_DEFINE_TYPE (GtkosxApplication, gtkosx_application, G_TYPE_OBJECT)

static GQuark emission_hook_quark = 0;

/*
 * parent_set_emission_hook:
 * @ihint: The signal hint confgigured when the signal was created.
 * @n_param_values: The number of parameters passed in param_values
 * @param_values: A GValue[] containing the parameters
 * data: A gpointer to pass to the signal handler
 *
 * Sets an emission hook for all parent-set signals.
 */
static gboolean
parent_set_emission_hook (GSignalInvocationHint *ihint,
                          guint                  n_param_values,
                          const GValue          *param_values,
                          gpointer               data)
{
  GtkWidget *instance = (GtkWidget*) g_value_get_object (param_values);

  if (GTK_IS_MENU_ITEM (instance))
    {
      GtkWidget *old_parent = (GtkWidget*) g_value_get_object (param_values + 1);
      GtkWidget *new_parent = gtk_widget_get_parent (instance);
      _GNSMenuItem *cocoa_item = cocoa_menu_item_get (instance);
      /* If neither the old parent or the new parent has a cocoa menu, then
         we're not really interested in this. */
      if (! ( (old_parent && GTK_IS_WIDGET (old_parent)
               && cocoa_menu_get (old_parent)) ||
              (new_parent && GTK_IS_WIDGET (new_parent)
               && cocoa_menu_get (new_parent))))
        return TRUE;

      if (GTK_IS_MENU_SHELL (old_parent))
        {
          _GNSMenuBar *cocoa_menu = (_GNSMenuBar*)cocoa_menu_get (old_parent);
[cocoa_item removeFromMenu: cocoa_menu];

        }
      /*This would be considerably more efficient if we could just
        insert it into the menu, but we can't easily get the item's
        position in the GtkMenu and even if we could we don't know that
        there isn't some other item in the menu that's been moved to the
        app-menu for quartz.  */
      if (GTK_IS_MENU_SHELL (new_parent) && cocoa_menu_get (new_parent))
        {
          _GNSMenuBar *cocoa_menu = (_GNSMenuBar*)cocoa_menu_get (new_parent);
          if (GTK_IS_MENU_BAR (new_parent) && cocoa_menu &&
    [cocoa_menu respondsToSelector: @selector (resync)])
            {
              [cocoa_menu resync];
            }
          else
            cocoa_menu_item_add_submenu (GTK_MENU_SHELL (new_parent),
                                         cocoa_menu,
                                         cocoa_menu == (NSMenu*) data,
                                         FALSE);
        }
    }
  return TRUE;
}

/*
 * parent_set_emission_hook_remove:
 * @widget: A random widget. Not used.
 * @data: A random gpointer. Not used.
 *
 * Removes the parent-set emission hook. This meets a particular
 * template, which is the reason for the unused parameters.
 */
static void
parent_set_emission_hook_remove (GtkWidget *widget,
                                 gpointer   data)
{
  gulong hook_id = (gulong)g_object_get_qdata (G_OBJECT (widget), emission_hook_quark);
  if (hook_id == 0) return;
  g_signal_remove_emission_hook (g_signal_lookup ("parent-set",
                                 GTK_TYPE_WIDGET),
                                 hook_id);
}

/*
 * add_to_menubar:
 * @self: The GtkosxApplication pointer.
 * @menu: The cocoa menu to add
 * pos: The position on the menubar to insert the new menu
 *
 * Add a submenu to the currently active main menubar.
 *
 * Returns: a pointer to the NSMenuItem now holding the menu.
 */
static _GNSMenuItem*
add_to_menubar (GtkosxApplication *self, NSMenu *menu, int pos)
{
  _GNSMenuItem *dummyItem = [[_GNSMenuItem alloc] initWithTitle: @""
			     action: nil keyEquivalent: @""];
  NSMenu *menubar = [NSApp mainMenu];

  [dummyItem setSubmenu: menu];
  if (pos < 0)
    [menubar addItem: dummyItem];
  else
    [menubar insertItem: dummyItem atIndex: pos];
  return dummyItem;
}

static NSString*
get_application_name (void)
{
  NSString *appname = nil;
  NSBundle *bundle = [NSBundle mainBundle];
  if ([bundle bundleIdentifier])
    {

      appname = [[bundle infoDictionary]
		 objectForKey: (NSString*)kCFBundleNameKey];
      if (appname == nil)
	appname = [[bundle infoDictionary]
		   objectForKey: @"CFBundleExecutable"];
      if (appname == nil)
	{
	  NSString *bundlep = [bundle bundlePath];
	  appname =  [[NSFileManager defaultManager]
		      displayNameAtPath: bundlep];
	  g_message ("[get_application_name]: no bundle name key in Info.plist\n");
	}
    }
  else
    {
      NSString *exepath = [bundle executablePath];
      appname =  [[NSFileManager defaultManager] displayNameAtPath: exepath];
    }
  return appname;
}

/*
 * create_apple_menu:
 * @self: The GtkosxApplication object.
 *
 * Creates the "app" menu -- the first one on the menubar with the
 * application's name. The function is called create_apple_menu
 * because of the undocumented Cocoa method to set it on the mainMenu.
 *
 * Note that the static strings are internationalized the Apple way,
 * so you'll need to use the Apple localization tools if you need to
 * translations other than the ones provided. The resource file will
 * be GtkosxApplication.strings, and must be installed in lang.proj in
 * the application bundle's Resources directory.
 *
 * Several menu items have the application name appended, in accord
 * with Apple's practice. When the app is bundled, it will look for
 * the Info.plist keys CFBundleName and CFBundleExecutable. If neither
 * exists (unlikely, since the app won't launch if CFBundleExecutable
 * isn't set), it will fall back to the name of the bundle, including
 * the ".app" extension if the user has checked "display all
 * extensions" in Finder>Preferences>Advanced. When not bundled, the
 * application name will be the executable name. This will generally
 * match what LaunchServices names the app menu.
 *
 * Returns: A pointer to the menu item.
 */
static _GNSMenuItem*
create_apple_menu (GtkosxApplication *self)
{
  NSMenuItem *menuitem;
  // Create the application (Apple) menu.
  NSMenu *app_menu = [[[NSMenu alloc] initWithTitle: @"Apple Menu"] autorelease];
  NSString *title = NSLocalizedStringFromTable (@"Services",
						@"GtkOSXApplication",
						@"Services Menu title");
  NSMenu *menuServices = [[[NSMenu alloc] initWithTitle: title] autorelease];
  NSString *appname = get_application_name ();
  [NSApp setServicesMenu: menuServices];

  [app_menu addItem: [NSMenuItem separatorItem]];
  menuitem = [[NSMenuItem alloc] initWithTitle:  NSLocalizedStringFromTable (@"Services",  @"GtkosxApplication", @"Services Menu Item title")
	      action: nil keyEquivalent: @""];
  [menuitem setSubmenu: menuServices];
  [app_menu addItem: menuitem];
  [menuitem release];
  [app_menu addItem: [NSMenuItem separatorItem]];
  menuitem = [[NSMenuItem alloc] initWithTitle: [[NSLocalizedStringFromTable (@"Hide",  @"GtkosxApplication", @"Hide menu item title") autorelease] stringByAppendingFormat: @" %@", appname]
	      action: @selector (hide: ) keyEquivalent: @"h"];
  [menuitem setTarget: NSApp];
  [app_menu addItem: menuitem];
  [menuitem release];
  menuitem = [[NSMenuItem alloc] initWithTitle: NSLocalizedStringFromTable (@"Hide Others",  @"GtkosxApplication", @"Hide Others menu item title")
	      action: @selector (hideOtherApplications: ) keyEquivalent: @"h"];
  [menuitem setKeyEquivalentModifierMask: NSCommandKeyMask | NSAlternateKeyMask];
  [menuitem setTarget: NSApp];
  [app_menu addItem: menuitem];
  [menuitem release];
  menuitem = [[NSMenuItem alloc] initWithTitle: NSLocalizedStringFromTable ( @"Show All", @"GtkosxApplication",  @"Show All menu item title")
	      action: @selector (unhideAllApplications: ) keyEquivalent: @""];
  [menuitem setTarget: NSApp];
  [app_menu addItem: menuitem];
  [menuitem release];
  [app_menu addItem: [NSMenuItem separatorItem]];
  menuitem = [[NSMenuItem alloc] initWithTitle: [[NSLocalizedStringFromTable (@"Quit",  @"GtkosxApplication", @"Quit menu item title") autorelease] stringByAppendingFormat: @" %@", appname]
	      action: @selector (terminate: ) keyEquivalent: @"q"];
  [menuitem setTarget: NSApp];
  [app_menu addItem: menuitem];
  [menuitem release];

  [NSApp performSelector: @selector (setAppleMenu: ) withObject: app_menu];
  return add_to_menubar (self, app_menu, 0);
}

/*
 * create_window_menu:
 * @self: The pointer to the GtkosxApplication object
 * @window: The toplevel window for which the menu is being created
 *
 * Creates the Window menu, the one which tracks the application's windows.
 *
 * Note that the static strings are internationalized the Apple way,
 * so you'll need to use the Apple localization tools if you need to
 * translations other than the ones provided. The resource file will
 * be GtkosxApplication.strings, and must be installed in lang.proj in
 * the application bundle's Resources directory.
 *
 * Returns: A pointer to the menu item on the mainMenu.
 */
static _GNSMenuItem *
create_window_menu (GtkosxApplication *self)
{
  NSString *title = NSLocalizedStringFromTable (@"Window",
                    @"GtkOSXApplication",
                    @"Window Menu title");
  NSMenu *window_menu = [[[NSMenu alloc] initWithTitle: title] autorelease];
  GtkMenuBar *menubar = [(_GNSMenuBar*)[NSApp mainMenu] menuBar];
  GtkWidget *parent = NULL;
  GdkWindow *win = NULL;
  NSWindow *nswin = NULL;
  int pos;

  g_return_val_if_fail (menubar != NULL, NULL);
  g_return_val_if_fail (GTK_IS_MENU_BAR (menubar), NULL);
  parent = gtk_widget_get_toplevel (GTK_WIDGET (menubar));
  if (parent && GTK_IS_WIDGET (parent))
    win = gtk_widget_get_window (parent);
  if (win && GDK_IS_WINDOW (win))
    nswin = gdk_quartz_window_get_nswindow (win);

  [window_menu addItemWithTitle: NSLocalizedStringFromTable (@"Minimize", @"GtkosxApplication", @"Windows|Minimize menu item")
   action: @selector (performMiniaturize: ) keyEquivalent: @"m"];
  [window_menu addItem: [NSMenuItem separatorItem]];
  [window_menu addItemWithTitle: NSLocalizedStringFromTable (@"Bring All to Front", @"GtkosxApplication", @"Windows|Bring All To Front menu item title")
   action: @selector (arrangeInFront: ) keyEquivalent: @""];

  [NSApp setWindowsMenu: window_menu];
  if (nswin)
    [NSApp addWindowsItem: nswin title: [nswin title] filename: NO];
  pos = [[NSApp mainMenu] indexOfItem: [(_GNSMenuBar*)[NSApp mainMenu] helpMenu]];
  return add_to_menubar (self, window_menu, pos);
}

/*
 * gtkosx_application_constructor:
 * @gtype: The GType of the new class
 * @n_properties: The number of properties passed in the next parameter
 * @properties: an array of construction properties
 *
 * Overrides the GObject (superclass) constructor to enforce a singleton
 * Note that the static strings are internationalized the Apple way,
 * so you'll need to use the Apple localization tools if you need to
 * translations other than the ones provided. The resource file will
 * be GtkosxApplication.strings, and must be installed in lang.proj in
 * the application bundle's Resources directory.
 *
 * Returns: A pointer to the new object.
 */
static GObject *
gtkosx_application_constructor (GType gtype,
                                guint n_properties,
                                GObjectConstructParam *properties)
{
  static GObject *self = NULL;
#if GLIB_CHECK_VERSION (2,32,0)
  static GMutex mutex;
  g_mutex_init (&mutex);
  g_mutex_lock (&mutex);
#else
  static GStaticMutex mutex = G_STATIC_MUTEX_INIT;
  g_static_mutex_lock (&mutex);
#endif
  if (self == NULL)
    {
      self = G_OBJECT_CLASS (gtkosx_application_parent_class)->constructor (gtype, n_properties, properties);
      g_object_add_weak_pointer (self, (gpointer) &self);

    }
#if GLIB_CHECK_VERSION (2,32,0)
  g_mutex_unlock (&mutex);
  g_mutex_clear (&mutex);
#else
  g_static_mutex_unlock (&mutex);
#endif
  return g_object_ref (self);

}

/*
 * g_cclosure_marshal_BOOLEAN__VOID:
 *
 * A private marshaller for handlers which take no parameters and
 * return a boolean.
 */
static void
g_cclosure_marshal_BOOLEAN__VOID (GClosure     *closure,
                                  GValue       *return_value G_GNUC_UNUSED,
                                  guint         n_param_values,
                                  const GValue *param_values,
                                  gpointer      invocation_hint G_GNUC_UNUSED,
                                  gpointer      marshal_data)
{
  typedef gboolean (*GMarshalFunc_BOOLEAN__VOID) (gpointer     data1,
      gpointer     data2);
  register GMarshalFunc_BOOLEAN__VOID callback;
  register GCClosure *cc = (GCClosure*) closure;
  register gpointer data1, data2;
  gboolean v_return;

  g_return_if_fail (n_param_values == 1);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = g_value_peek_pointer (param_values + 0);
    }
  else
    {
      data1 = g_value_peek_pointer (param_values + 0);
      data2 = closure->data;
    }
  callback = (GMarshalFunc_BOOLEAN__VOID) (marshal_data ? marshal_data : cc->callback);

  v_return = callback (data1, data2);
  g_value_set_boolean (return_value, v_return);
}


/*
 * g_cclosure_marshal_BOOLEAN__STRING:
 *
 * A private marshaller for handlers which take a string parameter and
 * return a boolean.
 */
static void
g_cclosure_marshal_BOOLEAN__STRING (GClosure     *closure,
                                    GValue       *return_value G_GNUC_UNUSED,
                                    guint         n_param_values,
                                    const GValue *param_values,
                                    gpointer      invocation_hint G_GNUC_UNUSED,
                                    gpointer      marshal_data)
{
  typedef gboolean (*GMarshalFunc_BOOLEAN__STRING) (gpointer     data1,
      const char     * arg1,
      gpointer     data2);
  register GMarshalFunc_BOOLEAN__STRING callback;
  register GCClosure *cc = (GCClosure*) closure;
  register gpointer data1, data2;
  gboolean v_return;

  g_return_if_fail (n_param_values == 2);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = g_value_peek_pointer (param_values + 0);
    }
  else
    {
      data1 = g_value_peek_pointer (param_values + 0);
      data2 = closure->data;
    }
  callback = (GMarshalFunc_BOOLEAN__STRING) (marshal_data ? marshal_data : cc->callback);

  v_return = callback (data1,
                       g_value_get_string (param_values + 1),
                       data2);
  g_value_set_boolean (return_value, v_return);
}

/*
 * g_cclosure_marshal_BOOLEAN__INT_BOOLEAN_BOOLEAN:
 *
 * A private marshaller for handlers which take an int and to boolean parameters and
 * return a boolean.
 */
static void
g_cclosure_marshal_BOOLEAN__INT_BOOLEAN_BOOLEAN (GClosure     *closure,
                                    GValue       *return_value G_GNUC_UNUSED,
                                    guint         n_param_values,
                                    const GValue *param_values,
                                    gpointer      invocation_hint G_GNUC_UNUSED,
                                    gpointer      marshal_data)
{
  typedef gboolean (*GMarshalFunc_BOOLEAN__INT_BOOLEAN_BOOLEAN) (gpointer     data1,
      const int arg1,
      const gboolean arg2,
      const gboolean arg3,
      gpointer     data2);
  register GMarshalFunc_BOOLEAN__INT_BOOLEAN_BOOLEAN callback;
  register GCClosure *cc = (GCClosure*) closure;
  register gpointer data1, data2;
  gboolean v_return;

  g_return_if_fail (n_param_values == 4);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = g_value_peek_pointer (param_values + 0);
    }
  else
    {
      data1 = g_value_peek_pointer (param_values + 0);
      data2 = closure->data;
    }
  callback = (GMarshalFunc_BOOLEAN__INT_BOOLEAN_BOOLEAN) (marshal_data ? marshal_data : cc->callback);

  v_return = callback (data1,
                       g_value_get_int (param_values + 1),
                       g_value_get_boolean (param_values + 2),
                       g_value_get_boolean (param_values + 3),
                       data2);
  g_value_set_boolean (return_value, v_return);
}

/*
 * block_termination_accumulator:
 *
 * A signal accumulator function for the NSApplicationShouldTerminate
 * signal.
 *
 * If a handler returns TRUE than we need to stop termination, so we
 * set the return value accumulator to TRUE and return FALSE (there's
 * no point in asking more handlers; we're going to abort the
 * shutdown). Otherwise, set the return value to FALSE (don't block
 * termination) and continue looking for handlers.
 *
 * Returns: gboolean
 */
static gboolean
block_termination_accumulator (GSignalInvocationHint *ihint, GValue *accum,
                               const GValue *retval, gpointer data)
{
  if (g_value_get_boolean (retval))
    {
      g_value_set_boolean (accum, TRUE);
      return FALSE; //Stop handling the signal
    }
  g_value_set_boolean (accum, FALSE);
  return TRUE; //Continue handling the signal
}

/*
 * global_event_filter_func
 * @windowing_event: The event to process as a gpointer
 * @event: The corresponding GdkEvent
 * @user_data: Pointer registerd with the signal handler.
 *
 * Processes Cocoa KeyEquivalents which don't have Gtk or application
 * implementations. In general, these must be KeyEquivalents like
 * command-q which are provided by the Cocoa framework.
 *
 * Returns: Whether to continue event processing.
 */
static GdkFilterReturn
global_event_filter_func (gpointer  windowing_event, GdkEvent *event,
                          gpointer  user_data)
{
  NSEvent *nsevent = windowing_event;
  GtkosxApplication* app = user_data;

  /* Handle menu events with no window, since they won't go through
   * the regular event processing. When using Gtk before 3.6, we have
   * to release the gdk mutex so that we can recquire it when we
   * invoke the gtk handler. Note well that handlers need to wrap any
   * calls into gtk in gdk_threads_enter() and gdk_threads_leave() in
   * a multi-threaded environment!
   *
   * With Gtk 3.6 and later, all Gtk calls must be made from the main
   * thread, so be sure that handlers aren't invoked on a worker
   * thread; use an idle or timer event to start the handler if
   * necessary.
   */
  if ([nsevent type] == NSKeyDown &&
      gtkosx_application_use_quartz_accelerators (app) )
    {
      gboolean result;
#if GTK_CHECK_VERSION (3, 6, 0)
      result = [[NSApp mainMenu] performKeyEquivalent: nsevent];
#else
      gdk_threads_leave ();
      result = [[NSApp mainMenu] performKeyEquivalent: nsevent];
      gdk_threads_enter ();
#endif
      if (result) return GDK_FILTER_TRANSLATE;
    }
  return GDK_FILTER_CONTINUE;
}

enum
{
  DidBecomeActive,
  WillResignActive,
  BlockTermination,
  WillTerminate,
  OpenFile,
  MultiMediaKey,
  LastSignal
};

static guint gtkosx_application_signals[LastSignal] = {0};
/*
 * gtkosx_application_init:
 * @self: The GtkosxApplication object.
 *
 * Class initialization. Includes creating a bunch of special signals
 * for echoing Cocoa signals through to the application.
 */
static void
gtkosx_application_init (GtkosxApplication *self)
{
  [NSApplication sharedApplication];
  self->priv = GTKOSX_APPLICATION_GET_PRIVATE (self);
  self->priv->use_quartz_accelerators = TRUE;
  self->priv->dock_menu = NULL;
  gdk_window_add_filter (NULL, global_event_filter_func, (gpointer)self);
  self->priv->notify = [[GtkApplicationNotificationObject alloc] init];
  [NSApp setDelegate: [GtkApplicationDelegate new]];
  self->priv->delegate = [NSApp delegate];
}

static void
gtkosx_application_dispose (GObject *obj)
{
  GtkosxApplication *self = GTKOSX_APPLICATION (obj);
  [self->priv->dock_menu release];
  [self->priv->notify release];
  [self->priv->delegate release];
  G_OBJECT_CLASS (gtkosx_application_parent_class)->dispose (obj);
}

/**
 * gtkosx_application_class_init:
 * @klass: The class type pointer
 *
 * Not normaly called directly; Use g_object_new(GTK_TYPE_OSXAPPLICATION)
 */
void
gtkosx_application_class_init (GtkosxApplicationClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  g_type_class_add_private (klass, sizeof (GtkosxApplicationPrivate));
  gobject_class->constructor = gtkosx_application_constructor;
  /**
   * GtkosxApplication::NSApplicationDidBecomeActive:
   * @app: The application object
   * #user_data: Data appended at connection
   *
   * Emitted by the Application Delegate when the application receives
   * an NSApplicationDidBecomeActive notification. Connect a handler if
   * there is anything you need to do when the application is activated.
   */
  gtkosx_application_signals[DidBecomeActive] =
    g_signal_new ("NSApplicationDidBecomeActive",
                  GTKOSX_TYPE_APPLICATION,
                  G_SIGNAL_NO_RECURSE | G_SIGNAL_ACTION,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
  /**
   * GtkosxApplication::NSApplicationWillResignActive:
   * @app: The application object
   * @user_data: Data appended at connection
   *
   * This signal is emitted by the Application Delegate when the
   * application receives an NSApplicationWillResignActive
   * notification. Connect a handler to it if there's anything your
   * application needs to do to prepare for inactivity.
   */
  gtkosx_application_signals[WillResignActive] =
    g_signal_new ("NSApplicationWillResignActive",
                  GTKOSX_TYPE_APPLICATION,
                  G_SIGNAL_NO_RECURSE | G_SIGNAL_ACTION,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /**
   * GtkosxApplication::NSApplicationBlockTermination:
   * @app: The application object
   * @user_data: Data appended at connection
   *
   * Emitted by the Application Delegate when the application reeeives
   * an NSApplicationShouldTerminate notification. Perform any cleanup
   * you need to do (e.g., saving files) before exiting. Returning FALSE
   * will allow further handlers to run and if none return TRUE, the
   * application to shut down. Returning TRUE will veto shutdown and
   * stop emission, so later handlers will not run.
   *
   * Returns: Boolean indicating that further emission and application
   * termination should be blocked.
   */
  gtkosx_application_signals[BlockTermination] =
    g_signal_new ("NSApplicationBlockTermination",
                  GTKOSX_TYPE_APPLICATION,
                  G_SIGNAL_NO_RECURSE | G_SIGNAL_ACTION,
                  0, block_termination_accumulator, NULL,
                  g_cclosure_marshal_BOOLEAN__VOID,
                  G_TYPE_BOOLEAN, 0);

  /**
   * GtkosxApplication::NSApplicationWillTerminate:
   * @app: The application object
   * @user_data: Data appended at connection
   *
   * Emitted by the Application Delegate when the application reeeives
   * an NSApplicationSWillTerminate notification. Connect your final
   * shutdown routine (the one that calls gtk_main_quit() here.
   */
  gtkosx_application_signals[WillTerminate] =
    g_signal_new ("NSApplicationWillTerminate",
                  GTKOSX_TYPE_APPLICATION,
                  G_SIGNAL_NO_RECURSE | G_SIGNAL_ACTION,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /**
   * GtkosxApplication::NSApplicationOpenFile:
   * @app: The application object
   * @path: A UTF8-encoded file path to open.
   * @user_data: Data attached at connection
   *
   * Emitted when a OpenFile, OpenFiles, or OpenEmptyFile event is
   * received from the operating system. This signal does not implement
   * drops, but it does implement "open with" events from Finder. An
   * OpenEmptyFile is received at launch in Python applications.
   *
   * Returns: Boolean indicating success at opening the file.
   */
  gtkosx_application_signals[OpenFile] =
    g_signal_new ("NSApplicationOpenFile",
                  GTKOSX_TYPE_APPLICATION,
                  G_SIGNAL_NO_RECURSE | G_SIGNAL_ACTION,
                  0, NULL, NULL,
                  g_cclosure_marshal_BOOLEAN__STRING,
                  G_TYPE_BOOLEAN, 1, G_TYPE_STRING);

  /**
   * GtkosxApplication::MultiMediaKey:
   * @app: The application object
   * @keyCode: code of the key
   * @isDown: is the key pressed
   * @repeat: key repeat?
   * @user_data: Data attached at connection
   *
   * Emitted when a multimedia key is pressed or released.
   *
   * Returns: Boolean indicating that the multimedia key should not be emitted.
   */
  gtkosx_application_signals[MultiMediaKey] =
    g_signal_new ("MultiMediaKey",
                  GTKOSX_TYPE_APPLICATION,
                  G_SIGNAL_NO_RECURSE | G_SIGNAL_ACTION,
                  0, NULL, NULL,
                  g_cclosure_marshal_BOOLEAN__INT_BOOLEAN_BOOLEAN,
                  G_TYPE_BOOLEAN, 3, G_TYPE_INT, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN);

}

/**
 * gtkosx_application_ready:
 * @self: The GtkosxApplication object
 *
 * Inform Cocoa that application initialization is complete.
 */
void
gtkosx_application_ready (GtkosxApplication *self)
{
  [NSApp finishLaunching];
}

/*
 * window_focus_cb:
 * @window: The application window receiving focus
 * @event: The GdkEvent. Not used.
 * @menubar: The _GNSMenubar associated with window
 *
 * Changes the active menubar when the application switches
 * windows. If you switch window focus programmatically, make sure
 * that the activate signal is emitted for the new window to trigger
 * this handler.
 */
static gboolean
window_focus_cb (GtkWindow* window, GdkEventFocus *event, _GNSMenuBar *menubar)
{
  [NSApp setMainMenu: menubar];
  return FALSE;
}

/**
 * gtkosx_application_set_menu_bar:
 * @self: The GtkosxApplication object
 * @menu_shell: The GtkMenuBar that you want to set.
 *
 * Set a window's menubar as the application menu bar. Call this once
 * for each window as you create them. It works best if the menubar is
 * reasonably fully populated before you call it. Once set, it will
 * stay syncronized through signals as long as you don't disconnect or
 * block them.
 */
void
gtkosx_application_set_menu_bar (GtkosxApplication *self, GtkMenuShell *menu_shell)
{
  _GNSMenuBar* cocoa_menubar;
  NSMenu* old_menubar = [NSApp mainMenu];
  GtkWidget *parent = gtk_widget_get_toplevel (GTK_WIDGET (menu_shell));
  gulong emission_hook_id;

  g_return_if_fail (GTK_IS_MENU_SHELL (menu_shell));

  cocoa_menubar = (_GNSMenuBar*)cocoa_menu_get (GTK_WIDGET (menu_shell));
  if (!cocoa_menubar)
    {
      cocoa_menubar = [[[_GNSMenuBar alloc] initWithGtkMenuBar:
                        GTK_MENU_BAR (menu_shell)] autorelease];
      cocoa_menu_connect (GTK_WIDGET (menu_shell), cocoa_menubar);
      /* turn off auto-enabling for the menu - its silly and slow and
         doesn't really make sense for a Gtk/Cocoa hybrid menu.
      */
      [cocoa_menubar setAutoenablesItems: NO];

    }
  if (cocoa_menubar != [NSApp mainMenu])
    [NSApp setMainMenu: cocoa_menubar];

  [cocoa_menubar setAppMenu: create_apple_menu (self)];

  emission_hook_id =
    g_signal_add_emission_hook (g_signal_lookup ("parent-set",
                                GTK_TYPE_WIDGET),
                                0,
                                parent_set_emission_hook,
                                cocoa_menubar, NULL);
  if (emission_hook_quark == 0)
    emission_hook_quark = g_quark_from_static_string ("GtkosxApplicationEmissionHook");
  g_object_set_qdata (G_OBJECT (menu_shell), emission_hook_quark,
                      (gpointer)emission_hook_id);

  g_signal_connect (parent, "focus-in-event",
                    G_CALLBACK (window_focus_cb),
                    cocoa_menubar);

  cocoa_menu_item_add_submenu (menu_shell, cocoa_menubar, TRUE, FALSE);
  /* Stupid hack to force the menubar to look right when a window is
     opened after starting the main loop. */
  if (old_menubar != NULL)
    {
      [NSApp setMainMenu: old_menubar];
      [NSApp setMainMenu: cocoa_menubar];
    }
}

/**
 * gtkosx_application_sync_menubar:
 * @self: The GtkosxApplication object
 *
 * Syncronize the active window's GtkMenuBar with the OSX menu
 * bar. You should only need this if you have programmatically changed
 * the menus with signals blocked or disconnected.
 */
void
gtkosx_application_sync_menubar (GtkosxApplication *self)
{
  [(_GNSMenuBar*)[NSApp mainMenu] resync];
}


/**
 * gtkosx_application_insert_app_menu_item:
 * @self: The GtkosxApplication object
 * @menu_item: The GtkMenuItem to add to the group.
 * @index: The place in the app menu that you want the item
 * inserted. The first item is 0.
 *
 * Certain menu items (About, Check for updates, and Preferences in
 * particular) are normally found in the so-called Application menu
 * (the first one on the menubar, named after the application) in OSX
 * applications. This function will create a menu entry for such a
 * menu item, removing it from its original menu in the Gtk
 * application.
 *
 * Any items inserted at index zero will have the app name appended;
 * this is intended for the "About" menu item, so that it says "About
 * Foo" like most Mac programs, without having to worry too much about
 * how "About" is spelled/translated.
 *
 * To group your menu items, insert GtkSeparatorMenuItem*s where you want them.
 *
 * Don't use it for Quit! A Quit menu item is created automatically
 * along with the Application menu. Just hide your Gtk Quit menu item.
 */
void
gtkosx_application_insert_app_menu_item (GtkosxApplication* self,
                                         GtkWidget* item,
                                         gint index)
{
  gtk_widget_set_visible (item, TRUE);
  /*Sleazy hack to avoid dealing with translating "About": If it's the
    zero'th item, it's the About item. */
  if (index == 0)
    {
      GtkWidget *widgetLabel = NULL;
      const gchar *label = get_menu_label_text (item, &widgetLabel);
      NSString *nsappname = get_application_name ();
      const gchar *appname = [nsappname UTF8String];
      gtk_menu_item_set_label (GTK_MENU_ITEM (item), g_strdup_printf ("%s %s", label, appname));
    }

  cocoa_menu_item_add_item ([[[NSApp mainMenu] itemAtIndex: 0] submenu],
                            item, index);
}

/**
 * gtkosx_application_set_window_menu:
 * @self: The application object
 * @menu_item: The menu item which will be set as the Window menu
 *
 * Sets a designated menu item already on the menu bar as the Window
 * menu. This is the menu which contains a list of open windows for
 * the application; by default it also provides menu items to minimize
 * and zoom the current window and to bring all windows to the
 * front. Call this after gtk_osx_application_set_menu_bar(). It
 * operates on the currently active menubar. If @nenu_item is NULL, it
 * will create a new menu for you, which will not be gettext translatable.
 */
void
gtkosx_application_set_window_menu (GtkosxApplication *self,
                                    GtkMenuItem *menu_item)
{
  _GNSMenuBar *cocoa_menubar = (_GNSMenuBar*)[NSApp mainMenu];
  g_return_if_fail (cocoa_menubar != NULL);

  if (menu_item)
    {
      GtkWidget *parent = NULL;
      GdkWindow *win = NULL;
      NSWindow *nswin = NULL;
      _GNSMenuItem *cocoa_item = cocoa_menu_item_get (GTK_WIDGET (menu_item));
      g_return_if_fail (cocoa_item != NULL);
      [cocoa_menubar setWindowsMenu: cocoa_item];
      [NSApp setWindowsMenu: [cocoa_item submenu]];
    }
  else
    {
      _GNSMenuItem *cocoa_item = create_window_menu (self);
      [cocoa_menubar setWindowsMenu:  cocoa_item];
    }
}

/**
 * gtkosx_application_set_help_menu:
 * @self: The application object
 * @menu_item: The menu item which will be set as the Window menu
 *
 * Sets a designated menu item already on the menu bar as the Help
 * menu. Call this after gtk_osx_application_set_menu_bar(), but
 * before gtk_osx_application_window_menu(), especially if you're
 * letting GtkosxApplication create a Window menu for you (it helps
 * position the Window menu correctly). It operates on the currently
 * active menubar. If @nenu_item is %NULL, it will create a new menu
 * for you, which will not be gettext translatable.
 */
void
gtkosx_application_set_help_menu (GtkosxApplication *self,
                                  GtkMenuItem *menu_item)
{
  _GNSMenuBar *cocoa_menubar = (_GNSMenuBar*)[NSApp mainMenu];
  g_return_if_fail (cocoa_menubar != NULL);

  if (menu_item)
    {
      _GNSMenuItem *cocoa_item = cocoa_menu_item_get (GTK_WIDGET (menu_item));
      g_return_if_fail (cocoa_item != NULL);
      [cocoa_menubar setHelpMenu: cocoa_item];
    }
  else
    {
      _GNSMenuItem *menuitem = [[[_GNSMenuItem alloc] initWithTitle: NSLocalizedStringFromTable(@"Help", @"GtkosxApplication", @"Help menu item title")
				 action: NULL keyEquivalent: @""] autorelease];
      [cocoa_menubar setHelpMenu: menuitem];
      [cocoa_menubar addItem: [cocoa_menubar helpMenu]];
    }
}

/* Dock support */
/* A bogus prototype to shut up a compiler warning. This function is for GtkApplicationDelegate and is not public. */
NSMenu* _gtkosx_application_dock_menu (GtkosxApplication *self);

/**
 * _gtkosx_application_dock_menu:
 * @self: The GtkosxApplication object.
 *
 * Return the dock menu to the Application Delegate; if not null, it
 * will be added to the dock menu.
 *
 * Returns: NSMenu*
 */
NSMenu*
_gtkosx_application_dock_menu (GtkosxApplication *self)
{
  return (self->priv->dock_menu);
}

/**
 * gtkosx_application_set_dock_menu:
 * @self: The GtkosxApplication object
 * @menu_shell: A GtkMenu (cast it with GTK_MENU_SHELL() when you
 * pass it in
 *
 * Set a GtkMenu as the dock menu.
 *
 * This menu does not have a "sync" function, so changes made while
 * signals are disconnected will not update the menu which appears in
 * the dock, and may produce strange results or crashes if a
 * GtkMenuItem or GtkAction associated with a dock menu item is
 * deallocated.
 */
void
gtkosx_application_set_dock_menu (GtkosxApplication *self,
                                  GtkMenuShell *menu_shell)
{
  g_return_if_fail (GTK_IS_MENU_SHELL (menu_shell));
  if (!self->priv->dock_menu)
    {
      self->priv->dock_menu = [[NSMenu alloc] initWithTitle: @""];
      cocoa_menu_item_add_submenu (menu_shell, self->priv->dock_menu,
				   FALSE, FALSE);
      [self->priv->dock_menu retain];
    }
}

/**
 * gtkosx_application_set_dock_icon_pixbuf:
 * @self: The GtkosxApplication
 * @pixbuf: The pixbuf. Pass NULL to reset the icon to its default.
 *
 * Set the dock icon from a GdkPixbuf
 */
void
gtkosx_application_set_dock_icon_pixbuf (GtkosxApplication *self,
    GdkPixbuf *pixbuf)
{
  if (!pixbuf)
    [NSApp setApplicationIconImage: nil];
  else
    [NSApp setApplicationIconImage: nsimage_from_pixbuf (pixbuf)];

}

/**
 * gtkosx_application_set_dock_icon_resource:
 * @self: The GtkosxApplication
 * @name: The ame of the image file
 * @type: The extension (e.g., jpg) of the filename
 * @subdir: The subdirectory of $Bundle/Contents/Resources in which to
 * look for the file.
 *
 * Set the dock icon from an image file in the bundle/
 */
void
gtkosx_application_set_dock_icon_resource (GtkosxApplication *self,
    const gchar  *name,
    const gchar  *type,
    const gchar  *subdir)
{
  NSImage *image = nsimage_from_resource (name, type, subdir);
  [NSApp setApplicationIconImage: image];
  [image release];
}

/**
 * gtkosx_application_attention_request:
 * @self: The GtkosxApplication pointer
 * @type: CRITICAL_REQUEST or INFO_REQUEST
 *
 * Create an attention request.  If type is CRITICAL_REQUEST, the
 * dock icon will bounce until cancelled the application receives
 * focus; otherwise it will bounce for 1 second -- but the attention
 * request will remain asserted until cancelled or the application
 * receives focus. This function has no effect if the application has focus.
 *
 * Returns: A the attention request ID. Pass this id to
 * gtkosx_application_cancel_attention_request.
 */
gint
gtkosx_application_attention_request (GtkosxApplication *self,
                                      GtkosxApplicationAttentionType type)
{
  return (gint)[NSApp requestUserAttention: (NSRequestUserAttentionType)type];
}

/**
 * gtkosx_application_cancel_attention_request:
 * @self: The application
 * @id: The integer attention request id returned from
 * gtkosx_application_attention_request.
 *
 * Cancel an attention request created with
 * gtkosx_application_attention_request.
 */
void
gtkosx_application_cancel_attention_request (GtkosxApplication *self, gint id)
{
  [NSApp cancelUserAttentionRequest: id];
}

/**
 * gtkosx_application_get_bundle_path:
 *
 * Return the root path of the bundle or the directory containing the
 *  executable if it isn't actually a bundle.
 *
 * Returns: path The bundle's absolute path or %NULL on error. g_free() it when done.
 */
gchar*
gtkosx_application_get_bundle_path (void)
{
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  gchar *str = NULL;
  NSString *path = [[NSBundle mainBundle] bundlePath];
  if (!path)
    return NULL;
  str = strdup ([path UTF8String]);
  [pool release];
  return str;
}

/**
 * gtkosx_application_get_bundle_id:
 *
 *Return the value of the CFBundleIdentifier key from the bundle's Info.plist
 *
 * This will return NULL if it's not really a bundle, there's no
 * Info.plist, or if Info.plist doesn't have a CFBundleIdentifier key
 * (So if you need to detect being in a bundle, make sure that your
 * bundle has that key!)
 *
 * Returns: The string value of CFBundleIdentifier, or %NULL if there is none. g_free() it when done.
 */
gchar*
gtkosx_application_get_bundle_id (void)
{
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  gchar *str = NULL;
  NSString *path = [[NSBundle mainBundle] bundleIdentifier];
  if (!path)
    return NULL;
  str = strdup ([path UTF8String]);
  [pool release];
  return str;
}

/**
 * gtkosx_application_get_resource_path:
 *
 * Return the Resource path for the bundle or the directory containing the
 *  executable if it isn't actually a bundle. Use gtkosx_application_get_bundle_id() to check (it will return %NULL if it's not a bundle).
 *
 * Returns: path The absolute resource path. or %NULL on error. g_free() it when done.
 */
gchar*
gtkosx_application_get_resource_path (void)
{
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  gchar *str = NULL;
  NSString *path = [[NSBundle mainBundle] resourcePath];
  if (!path)
    return NULL;
  str = strdup ([path UTF8String]);
  [pool release];
  return str;
}


/**
 * gtkosx_application_get_executable_path:
 *
 * Return the executable path, including file name
 *
 * Returns: The path to the primary executable, or %NULL if it can't find one. g_free() it when done
 */
gchar*
gtkosx_application_get_executable_path (void)
{
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  gchar *str = NULL;
  NSString *path = [[NSBundle mainBundle] executablePath];
  if (!path)
    return NULL;
  str = strdup ([path UTF8String]);
  [pool release];
  return str;
}

/**
 * gtkosx_application_get_bundle_info:
 * @key: The key, as a normal UTF8 string.
 *
 * Queries the bundle's Info.plist with key. If the returned object is
 * a string, returns that; otherwise returns %NULL.
 *
 * Returns: A UTF8-encoded string. g_free() it when done.
 */
gchar*
gtkosx_application_get_bundle_info (const gchar *key)
{
  gchar *result = NULL;
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  NSObject *id = [[NSBundle mainBundle] objectForInfoDictionaryKey:
		  [NSString stringWithUTF8String: key]];

  if ([id respondsToSelector: @selector (UTF8String)])
    {
      result = g_strdup ([(NSString*)id UTF8String]);
    }

  [pool release];
  return result;
}
