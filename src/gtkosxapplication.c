/* GTKOSXApplication Class
 *
 * Copyright © 2010 John Ralls
 *
 * An application class which mirrors the Cocoa NSApplication to
 * receive signals from the GtkApplicationDelegate class in
 * gtkosxapplication_quartz.m
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

#include "gtkosxapplication.h"
#include "gtkosxapplicationprivate.h"

//#define DEBUG(format, ...) g_printerr ("%s: " format, G_STRFUNC, ## __VA_ARGS__)
#define DEBUG(format, ...)
/**
 * SECTION:gtkosxapplication
 * @short_description: Base class for OSX integration
 * @title: GtkosxApplication
 * @include: gtkosxapplication.h
 *
 * Exposes to the Gtk+ program important functions of
 * OSX's NSApplication class for use by Gtk+ applications running with
 * the quartz Gdk backend and provides addtional functions for
 * integrating a Gtk+ program into the OSX user environment.
 *
 * Using GtkosxApplication is pretty simple.
 * First, create an instance at startup:
 *
 * |[GtkosxApplication *theApp = g_object_new(GTKOSX_TYPE_APPLICATION, NULL);]|
 *
 * Do this early in your program, shortly after you run
 * |[gtk_init()]|. Don't forget to guard it, and all other calls into
 * the library, with |[#ifdef MAC_INTEGRATION]|. You don't want your
 * Linux users' builds failing because of this.  The application
 * object is a singleton, so you can call g_object_new as often as you
 * like. You'll always get the same pointer back. There's no need to
 * pass it around as an argument. Do note that all of the
 * GtkosxApplication functions take theApp as an argument, even if
 * they don't use it. This seems silly in C, and perhaps it is, but
 * it's needed to make the Python binding logic recognize that they're
 * class methods.
 *
 * Just having the application object created will get you some
 * benefits, like having the Quit menu item in the dock menu work. But
 * you'll obviously want more. So the next place to visit is your main
 * window code. If you have a simple application, you might be
 * constructing the menu by hand, but you're more likely to be using
 * GtkBuilder. In either case, you need to get a pointer to the
 * menubar. If you're building by hand, you've already got it lying
 * around because you needed it to add the menus to. With GtkBuilder,
 * you need to ask the GtkUIManager for a pointer. Once everything is
 * more-or-less set up on the Gtk+ side, you need only hide the menu
 * and call gtkosx_application_set_main_menu(). Here's an example with
 * GtkBuilder:
 *
 * <example>
 * <title>Setting the MenuBar</title>
 * <programlisting>
 *   GtkUIManager *mgr = gtk_ui_manager_new();
 *   GtkosxApplication *theApp = g_object_new(GTKOSX_TYPE_APPLICATION, NULL);
 *   ...
 *   mergeid = gtk_ui_manager_add_ui_from_file(mgr, "src/testui.xml", &err);
 *   ...
 *   menubar = gtk_ui_manager_get_widget(mgr, "/menubar");
 *   gtk_widget_hide (menubar);
 *   gtkosx_application_set_menu_bar(theApp, GTK_MENU_SHELL(menubar));
 * </programlisting>
 * </example>
 *
 * There are a couple of wrinkles, though, if you use
 * accelerators. First off, there are two event paths for
 * accelerators: Quartz, where the keystroke is processed by OSX and
 * the menu item action event is placed on the event queue by OSX, or
 * Gtk, where the accelerator key event is passed through to Gtk to
 * recognize. This is controlled by
 * gtkosx_application_set_use_quartz_accelerators() (you can test the
 * value with gtkosx_application_use_quartz_accelerators()), and the
 * default is to use Quartz handling. This has two advantages:
 * <itemizedlist>
 * <listitem><para>It works without any extra steps</para></listitem>
 * <listitem><para>
 * It changes stock accelerators (like Ctrl-O for open file) to
 * the stock OSX keyEquivalent (Cmd-O in that case).
 * </para></listitem>
 * </itemizedlist>
 * If you need to use Gtk+ keyboard accelerator handling <emphasis>and</emphasis>
 * you're using GtkMenuItems instead of GtkActions, you'll need to
 * connect a special handler as shown in the following example:
 * <example>
 * <title>Enabling Accelerators on Hidden Menus</title>
 * <programlisting>
 * static gboolean
 * can_activate_cb(GtkWidget* widget, guint signal_id, gpointer data)
 * {
 *   return gtk_widget_is_sensitive(widget);
 * }
 * ...
 *   g_signal_connect(menubar, "can-activate-accel",
 *                    G_CALLBACK(can_activate_cb), NULL);
 * </programlisting>
 * </example>
 *
 * The next task to make your application appear more normal for Mac
 * users is to move some menu items from their normal Gtk locations to
 * the so-called "App" menu. That's the menu all the way at the left
 * of the menubar that has the currently-running application's
 * name. There are 3 menu items that normally go there:
 * <itemizedlist>
 * <listitem><para>Help|About</para></listitem>
 * <listitem><para>Edit|Preferences</para></listitem>
 * <listitem><para>File|Quit</para></listitem>
 * </itemizedlist>
 * File|Quit is a special case, because OSX handles it itself and
 * automatically includes it, so the only thing you need do is hide it
 * on the File menu so that it doesn't show up twice:
 * |[gtk_widget_hide(GTK_WIDGET(file_quit_menu_item));]|
 * The other two must be moved in code, and there are two functions
 * for doing that. The first one creates "goups", which is just an
 * easy way to manage separators, and the second adds the actual menu
 * items to the groups. Here's an example:
 * <example>
 * <programlisting>
 *  GtkosxApplicationMenuGroup *group;
 *  GtkMenuItem *about_item, *preferences_item;
 *  about_item = gtk_ui_manager_get_widget(mgr, "/menubar/Help/About");
 *  preferences_item = gtk_ui_manager_get_widget(mgr, "/menubar/Edit/Preferences");
 *
 *  group = gtkosx_application_add_app_menu_group (theApp);
 *  gtkosx_application_add_app_menu_item  (theApp, group,
 *                                         GTK_MENU_ITEM (about_item));
 *
 *  group = gtkosx_application_add_app_menu_group (theApp);
 *  gtkosx_application_add_app_menu_item  (theApp, group,
 *                                         GTK_MENU_ITEM (preferences_item));
 * </programlisting>
 * </example>
 * Once we have everything set up for as many windows as we're going
 * to open before we call gtk_main_loop(), we need to tell OSX that
 * we're ready:
 * |[gtkosx_application_ready(theApp);]|
 *
 * If you add other windows later, you must do everything above for
 * each one's menubar. Most of the time the internal notifictations
 * will ensure that the GtkosxApplication is able to keep everything
 * in sync. However, if you at any time disconnect or block signals
 * and change the menu (perhaps because of a context change within a
 * window, as with changing pages in a GtkNotebook) you need to call
 * |[gtkosx_application_sync_menubar(theApp)]|
 *
 *
 * * The dock is that bar of icons that normally lives at the bottom of
 * the display on a Mac (though it can be moved to one of the other
 * sides; this author likes his on the left, which is where it was
 * originally on a NeXT). Each running application has a "dock tile",
 * an icon on the dock. Users can, if they like, add application (or
 * document) icons to the dock, and those can be used to launch the
 * application. Apple allows limited customization of the dock tile,
 * and GtkosxApplication has an interface for adding to the dock's
 * menu and for changing the icon that is displayed for the the
 * application. GtkosxApplication also provides an interface to
 * AttentionRequest, which bounces the dock tile if the application
 * doesn't have focus. You might want to do that at the end of a long
 * task so that the user will know that it's finished if she's
 * switched to another application while she waits for yours.
 * They're all pretty simple, so you can just read the details below.
 *
 *
 * * The last feature to which GtkosxApplication provides an interface
 * is the bundle. Normally in OSX, graphical applications are packaged
 * along with their non-standard dependencies and their resources
 * (graphical elements, translations, and such) in special directory
 * structures called "bundles". To easily package your Gtk+
 * application, have a look at gtk-mac-bundler, also available from
 * the Gtk-OSX project.
 *
 * OSX provides a variety of functions pertaining to bundles, most of which are not likely to interest someone porting a Gtk+ application. GtkosxApplication has wrapped a few that might be:
 * <itemizedlist>
 * <listitem><para>gtkosx_application_get_bundle_path()</para></listitem>
 * <listitem><para>gtkosx_application_get_resource_path()</para></listitem>
 * <listitem><para>gtkosx_application_get_executable_path()</para></listitem>
 * <listitem><para>gtkosx_application_get_bundle_id()</para></listitem>
 * <listitem><para>gtkosx_application_get_bundle_info()</para></listitem>
 * </itemizedlist>
 *
 * The first three just get a UTF8-encoded path. An interesting note
 * is that they'll return the path to the executable or the folder
 * it's in regardless of whether it's actually in a bundle. To find
 * out if one is actually dealing with a bundle,
 * gtkosx_application_get_bundle_id() will return "" if it can't find
 * the key %CFBundleIdentifier from the bundle's Info.plist -- which it
 * won't if the application isn't in a bundle or wasn't launched by
 * opening the bundle. (In other words, even if you have your
 * application installed in Foo.app, if you launch it from the command
 * line as
 *|[$ Foo.app/Contents/MacOS/Foo]|
 * the Info.plist won't have been opened and
 * gtkosx_application_get_bundle_id() will return "". Of course, it
 * will also return "" if you didn't set %CFBundleIdentifier in the
 * Info.plist, so make sure that you do!
 *
 * The last function, gtkosx_application_get_bundle_info(), will
 * return the value associated with an arbitrary key from Info.plist
 * as long as that value is a string. If it isn't, then the function
 * returns a null string ("").
 *
 * Finally, notice the signals. These are emitted in response to the
 * indicated OSX notifications. Except for
 * #GtkosxApplication::NSApplicationBlockTermination, most programs
 * won't need to do anything with
 * them. #GtkosxApplication::NSApplicationBlockTermination is telling
 * you that OSX is planning to shut down your program. If you have any
 * cleanup to do (like saving open files), or if you want to ask the
 * user if it's OK, you should connect to the signal and do your
 * cleanup. Your handler can return %TRUE to prevent the application
 * from quitting.
 */

/**
 * gtkosx_application_get:
 *
 * Returns: (transfer none) the singleton application object.
 */
GtkosxApplication *
gtkosx_application_get (void)
{
  return g_object_new (GTKOSX_TYPE_APPLICATION, NULL);
}


/**
 * gtkosx_application_use_quartz_accelerators:
 * @self: The GtkosxApplication pointer.
 *
 * Are we using Quartz or Gtk+ accelerator handling?
 *
 * Returns: a gboolean
 */
gboolean
gtkosx_application_use_quartz_accelerators (GtkosxApplication *self)
{
  return self->priv->use_quartz_accelerators;
}

/**
 * gtkosx_application_set_use_quartz_accelerators:
 * @self: The GtkosxApplication pointer.
 * @use_quartz_accelerators: Gboolean
 *
 * Set quartz accelerator handling; TRUE (default) uses quartz; FALSE
 * uses Gtk+. Quartz accelerator handling is required for normal OSX
 * accelerators (e.g., command-q to quit) to work.
 */
void
gtkosx_application_set_use_quartz_accelerators (GtkosxApplication *self,
    gboolean use_quartz_accelerators)
{
  self->priv->use_quartz_accelerators = use_quartz_accelerators;
}

/*
 * gtk_type_osxapplication_attention_type_get_type:
 *
 * A public enum used to set the parameter for attention
 * requests. Exists soley to satisfy the PyGObject codegen system.
 */
GType
gtkosx_type_application_attention_type_get_type (void)
{
  //Bogus GType, but there's no good reason to register this; it's only an enum
  return GTKOSX_TYPE_APPLICATION;
}

/**
 * gtkosx_application_listen_for_multimedia_keys:
 * @self: The GtkosxApplication pointer.
 *
 * Is the multimedia keys event tap active?
 *
 * Returns: a gboolean
 */
gboolean
gtkosx_application_listen_for_multimedia_keys (GtkosxApplication *self)
{
  return self->priv->listen_for_multimedia_keys;
}


/**
 * gtkosx_application_set_listen_for_multimedia_keys:
 * @self: The GtkosxApplication pointer.
 * @listen_for_multimedia_keys: Gboolean
 *
 * Register a global event tap to listen to multimedia keys and be able to inhibit
 * them; TRUE activate the event tap; FALSE (default) won't activate the
 * event tap.
 *
 * This can't be changed after gtkosx_application_ready is called
 */
void
gtkosx_application_set_listen_for_multimedia_keys (GtkosxApplication *self,
    gboolean listen_for_multimedia_keys)
{
  self->priv->listen_for_multimedia_keys = listen_for_multimedia_keys;
}
