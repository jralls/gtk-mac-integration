/* GTK+ Integration for the Mac OS X.
 *
 * Copyright (C) 2007 Imendio AB
 * Copyright © 2009, 2010 John Ralls
 *
 * For further information, see:
 * http://sourceforge.net/apps/trac/gtk-osx/wiki/Integrate
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
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

/* - Dock menu (see
 *   https://bugzilla.mozilla.org/show_bug.cgi?id=377166) It looks
 *   like we have to use NSMenu for this unfortunately. However, it
 *   might be possible to implement a category or to use the "method
 *   swizzling" trick to override this somehow... look into that.
 *
 *   or, triggering the carbon dock setup before sharedapp:
 *
 *     EventTypeSpec kFakeEventList[] = { { INT_MAX, INT_MAX } };
       EventRef event;
       ReceiveNextEvent (GetEventTypeCount (kFakeEventList),
                         kFakeEventList,
                         kEventDurationNoWait, false,
                         &event);
 *
 * - Dragging items onto the dock icon?
 * - Dragging items onto the app launcher icon?
 *
 * - Handling many windows with menus...?
 *
 * - Better statusicon integration (real menu)
 *
 * - "Window" menu, add a way to add a standard Window menu.
 * - Window listing in the dock menu.
 *
 * - Implement moving borderless windows by dragging the window
 *   background.
 *
 * - Suspend/resume notification.
 * - Network on/off notification.
 * - GtkCheckMenuItems
 * - GtkRadioMenuItems (special case of GtkCheckMenuItems)
 */

#include <gtk/gtk.h>
#include <stdio.h>

/* Uncomment ONE of these to test menu-mangling: */
//#define GTKMACINTEGRATION
#define GTKOSXAPPLICATION

/* These others are optional */
#define GTK_DISABLE_DEPRECATION_WARNINGS 1
#define BUILT_UI //The built UI uses deprecated functions
//#define QUARTZ_HANDLERS

/* GTKMACINTEGRATION uses Carbon, which isn't available for 64-bit builds. */
#ifdef __x86_64__
#undef GTKMACINTEGRATION
# ifndef GTKOSXAPPLICATION
#define GTKOSXAPPLICATION
# endif
#endif //__x86_64__

#if GTK_CHECK_VERSION (2, 90, 7)
#include <gdk/gdkkeysyms-compat.h>
#else
#include <gdk/gdkkeysyms.h>
#endif

#ifdef GTKMACINTEGRATION
#include "gtk-mac-menu.h"
#include "gtk-mac-dock.h"
#include "gtk-mac-bundle.h"
#endif
#ifdef GTKOSXAPPLICATION
#include "gtkosxapplication.h"
#endif
#include <config.h>

typedef struct
{
  GtkWindow *window;
  GtkWidget *open_item;
  GtkWidget *edit_item;
  GtkWidget *copy_item;
  GtkWidget *quit_item;
  GtkWidget *window_menu;
  GtkWidget *help_menu;
  GtkWidget *about_item;
  GtkWidget *preferences_item;
} MenuItems;

static GQuark menu_items_quark = 0;

static MenuItems *
menu_items_new ()
{
  return g_slice_new0 (MenuItems);
}

static void
menu_items_destroy (MenuItems *items)
{
  g_slice_free (MenuItems, items);
}

typedef struct
{
  gchar *label;
  gpointer item;
} MenuCBData;

static MenuCBData *
menu_cbdata_new (gchar *label, gpointer item)
{
  MenuCBData *datum =  g_slice_new0 (MenuCBData);
  datum->label = label;
  datum->item = item;
  g_object_ref (datum->item);
  return datum;
}

static void
menu_cbdata_delete (MenuCBData *datum)
{
  g_object_unref (datum->item);
  g_slice_free (MenuCBData, datum);
}

static void
menu_item_activate_cb (GtkWidget *item,
                       MenuCBData  *datum)
{
  gboolean visible;
  gboolean sensitive;
  MenuItems *items = g_object_get_qdata (G_OBJECT (datum->item),
                                         menu_items_quark);
  if (GTK_IS_WINDOW (G_OBJECT (datum->item)))
    g_print ("Item activated: %s:%s\n",
             gtk_window_get_title (GTK_WINDOW (datum->item)),
             datum->label);
  else
    g_print ("Item activated %s\n", datum->label);

  if (!items)
    return;

  g_object_get (G_OBJECT (items->copy_item),
                "visible", &visible,
                "sensitive", &sensitive,
                NULL);

  if (item == items->open_item)
    {
      gtk_widget_set_sensitive (items->copy_item,
                                !gtk_widget_get_sensitive (items->copy_item));
    }
}

#ifdef BUILT_UI
static GtkWidget *create_window (const gchar *title);

static void
action_activate_cb (GtkAction* action, gpointer data)
{
  GtkWindow *window = data;
  g_print ("Window %s, Action %s\n", gtk_window_get_title (window),
           gtk_action_get_name (action));
}

static void
new_window_cb (GtkAction* action, gpointer data)
{
  static guint serial = 2;
  gchar *title;
  g_print ("Create New Window\n");
  title = g_strdup_printf ( "Test Integration Window %d", serial++);
  create_window (title);
  g_free (title);

}

static void
minimize_cb (GtkAction *action, gpointer data)
{
  g_return_if_fail (data != NULL);
  gtk_window_iconify (GTK_WINDOW (data));
}

static void
front_cb (GtkAction *action, gpointer data)
{
  g_return_if_fail (data != NULL);
}

static GtkActionEntry test_actions[] =
{
  /*{Name, stock_id, label, accelerator, tooltip, callback} */
  {"FileMenuAction", NULL, "_File", NULL, NULL, NULL},
  {
    "OpenAction",  GTK_STOCK_OPEN, "_Open", NULL, NULL,
    G_CALLBACK (action_activate_cb)
  },
  {
    "QuitAction", GTK_STOCK_QUIT, "_Quit", NULL, NULL,
    G_CALLBACK (gtk_main_quit)
  },
  {"EditMenuAction", NULL, "_Edit", NULL, NULL, NULL },
  {"WindowsMenuAction", NULL, "_Window", NULL, NULL, NULL },
  {
    "CopyAction", GTK_STOCK_COPY, "_Copy", NULL, NULL,
    G_CALLBACK (action_activate_cb)
  },
  {
    "PasteAction", GTK_STOCK_PASTE, "_Paste", NULL, NULL,
    G_CALLBACK (action_activate_cb)
  },
  {
    "PrefsAction", GTK_STOCK_PREFERENCES, "Pr_eferences", NULL, NULL,
    G_CALLBACK (action_activate_cb)
  },
  {"MinimizeAction", NULL, "_Minimize", "<meta>m", NULL, G_CALLBACK (minimize_cb)},
  {"FrontAction", NULL, "Bring All to Front", NULL, NULL, G_CALLBACK (front_cb)},
  {
    "AddWindowAction", NULL, "_Add Window", NULL, NULL,
    G_CALLBACK (new_window_cb)
  },
  {"HelpMenuAction", NULL, "_Help", NULL, NULL, NULL },
  {
    "AboutAction", GTK_STOCK_ABOUT, "_About", NULL, NULL,
    G_CALLBACK (action_activate_cb)
  },
  {
    "HelpAction", GTK_STOCK_HELP, "_Help", NULL, NULL,
    G_CALLBACK (action_activate_cb)
  },
};

static void
radio_item_changed_cb (GtkAction* action, GtkAction* current, MenuCBData *datum)
{
  g_print ("Radio group %s in window %s changed value: %s is now active.\n",
           datum->label, gtk_window_get_title (GTK_WINDOW (datum->item)),
           gtk_action_get_name (GTK_ACTION (current)));
}

static GtkActionEntry view_menu[] =
{
  {"ViewMenuAction", NULL, "_View", NULL, NULL, NULL},
};

static GtkRadioActionEntry view_actions[] =
{
  /* Name, StockID, Label, Accelerator, Tooltip, Value */
  {"HorizontalAction", NULL, "_Horizontal", NULL, NULL, 0},
  {"VerticalAction", NULL, "_Vertical", NULL, NULL, 0},
};
#else //not BUILT_UI
# if !defined QUARTZ_HANDLERS && defined GTKOSXAPPLICATION

/* This is needed as a callback to enable accelerators when not using
 * the Quartz event handling path and using GtkMenuItems instead of
 * GtkActions, otherwise hiding the menu disables accelerators. */

static gboolean
can_activate_cb (GtkWidget* widget, guint signal_id, gpointer data)
{
  return gtk_widget_is_sensitive (widget);
}
# endif //!QUARTZ_HANDLERS

static GtkWidget *
test_setup_menu (MenuItems *items, GtkAccelGroup *accel)
{
  GtkWidget *menubar;
  GtkWidget *menu;
  GtkWidget *item;
  menubar = gtk_menu_bar_new ();

  item = gtk_menu_item_new_with_label ("File");
  gtk_menu_shell_append (GTK_MENU_SHELL (menubar), item);
  menu = gtk_menu_new ();
  gtk_menu_set_accel_group (GTK_MENU (menu), accel);
  gtk_menu_set_accel_path (GTK_MENU (menu), "<test-integration>/File");
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), menu);
#if GTK_CHECK_VERSION (3, 10, 0)
  item = gtk_menu_item_new_with_label ("Open");
#else
  item = gtk_image_menu_item_new_from_stock (GTK_STOCK_OPEN, NULL);
#endif
  items->open_item = item;
  /* We're being fancy with our connection here so that we don't have to
   * have a separate callback function for each menu item, since each
   * one is going to print out a message saying what item got
   * selected. A real-life menu item usually uses just
   * g-signal_connect() to a dedicated callback -- or uses GuiManager
   * action closures.
   */
  g_signal_connect_data (item, "activate", G_CALLBACK (menu_item_activate_cb),
                         menu_cbdata_new ("open", items->window),
                         (GClosureNotify) menu_cbdata_delete, 0);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
#if GTK_CHECK_VERSION (3, 10, 0)
  items->quit_item = gtk_menu_item_new_with_label ("Quit");
#else
  items->quit_item = gtk_image_menu_item_new_from_stock (GTK_STOCK_QUIT, NULL);
#endif
  g_signal_connect (items->quit_item, "activate", G_CALLBACK (gtk_main_quit), NULL);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), items->quit_item);
//Set accelerators
  gtk_accel_map_add_entry ("<test-integration>/File/Open", GDK_o,
                           GDK_CONTROL_MASK);
  gtk_accel_map_add_entry ("<test-integration>/File/Quit", GDK_q,
                           GDK_CONTROL_MASK);
  items->edit_item = item = gtk_menu_item_new_with_label ("Edit");

  gtk_menu_shell_append (GTK_MENU_SHELL (menubar), item);
  menu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), menu);

  item = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

  item = gtk_menu_item_new_with_label ("Copy");
  items->copy_item = item;
  g_signal_connect_data (item, "activate", G_CALLBACK (menu_item_activate_cb),
                         menu_cbdata_new ("copy", items->window),
                         (GClosureNotify) menu_cbdata_delete, 0);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

  item = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

  item = gtk_menu_item_new_with_label ("Paste");
  g_signal_connect_data (item, "activate", G_CALLBACK (menu_item_activate_cb),
                         menu_cbdata_new ( "paste", items->window),
                         (GClosureNotify) menu_cbdata_delete, 0);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

  item = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

  items->preferences_item = gtk_menu_item_new_with_label ("Preferences");
  g_signal_connect_data (items->preferences_item, "activate",
                         G_CALLBACK (menu_item_activate_cb),
                         menu_cbdata_new ("preferences", items->window),
                         (GClosureNotify) menu_cbdata_delete, 0);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), items->preferences_item);

  item = gtk_menu_item_new_with_label ("Help");
  items->help_menu = item;
  gtk_menu_shell_append (GTK_MENU_SHELL (menubar), item);
  menu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), menu);

#if GTK_CHECK_VERSION (3, 10, 0)
  items->about_item = gtk_menu_item_new_with_label ("About");
#else
  items->about_item = gtk_image_menu_item_new_from_stock (GTK_STOCK_ABOUT,
							  NULL);
#endif
  g_signal_connect_data (items->about_item, "activate",
                         G_CALLBACK (menu_item_activate_cb),
                         menu_cbdata_new ("about", items->window),
                         (GClosureNotify) menu_cbdata_delete, 0);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), items->about_item);

  return menubar;
}
#endif //not BUILT_UI

#ifdef GTKMACINTEGRATION
static void
dock_clicked_cb (GtkMacDock *dock,
                 GtkWindow  *window)
{
  g_print ("Dock clicked\n");
  gtk_window_deiconify (window);
}

static void
bounce_cb (GtkWidget  *button,
           GtkMacDock *dock)
{
  gtk_mac_dock_attention_request (dock, GTK_MAC_ATTENTION_INFO);
}

static void
change_icon_cb (GtkWidget  *button,
                GtkMacDock *dock)
{
  static gboolean   changed;
  static GdkPixbuf *pixbuf;

  if (!pixbuf)
    {
      char filename[PATH_MAX];
      snprintf (filename, sizeof (filename), "%s/%s", PREFIX,
                "share/gtk-2.0/demo/gnome-foot.png");
      pixbuf = gdk_pixbuf_new_from_file (filename, NULL);
    }

  if (changed)
    gtk_mac_dock_set_icon_from_pixbuf (dock, NULL);
  else
    gtk_mac_dock_set_icon_from_pixbuf (dock, pixbuf);

  changed = !changed;
}
#elif defined GTKOSXAPPLICATION
typedef struct
{
  GtkosxApplication *app;
  GtkosxApplicationAttentionType type;
} AttentionRequest;

static gboolean
attention_cb (AttentionRequest* req)
{
  gtkosx_application_attention_request (req->app, req->type);
  g_free (req);
  return FALSE;
}

static void
bounce_cb (GtkWidget  *button,
           GtkosxApplication *app)
{
  AttentionRequest *req = g_new0 (AttentionRequest, 1);
  req->app = app;
  req->type = CRITICAL_REQUEST;
  g_timeout_add_seconds (2, (GSourceFunc)attention_cb, req);
  g_print ("Now switch to some other application\n");
}

static void
change_icon_cb (GtkWidget  *button,
                GtkosxApplication *app)
{
  static gboolean   changed;
  static GdkPixbuf *pixbuf;
  if (!pixbuf)
    {
      char filename[PATH_MAX];
      snprintf (filename, sizeof (filename), "%s/%s", PREFIX,
                "share/gtk-2.0/demo/gnome-foot.png");
      pixbuf = gdk_pixbuf_new_from_file (filename, NULL);
    }

  if (changed)
    gtkosx_application_set_dock_icon_pixbuf (app, NULL);
  else
    gtkosx_application_set_dock_icon_pixbuf (app, pixbuf);

  changed = !changed;
}
#else //Neither GTKMACINTEGRATION nor GTKOSXAPPLICATION
static gboolean
attention_cb (gpointer req)
{
}
static void
bounce_cb (GtkWidget  *button,
           gpointer app)
{
}
static void
change_icon_cb (GtkWidget  *button,
                gpointer app)
{
}
#endif //GTKMACINTEGRATION
static void
toggle_prefs_cb (GtkWidget *button,
		 gpointer user_data)
{
  GtkWidget *window = gtk_widget_get_toplevel (button);
  MenuItems *items = g_object_get_qdata (G_OBJECT (window), menu_items_quark);
  gboolean state = gtk_widget_get_sensitive (GTK_WIDGET (items->preferences_item));
  g_print ("Setting Preferences Item sensitive to %s\n",
	   state ? "False" : "True");
  gtk_widget_set_sensitive (GTK_WIDGET (items->preferences_item), !state);
}

static void
change_menu_cb (GtkWidget  *button,
                gpointer    user_data)
{
  GtkWidget *window = gtk_widget_get_toplevel (button);
  MenuItems *items = g_object_get_qdata (G_OBJECT (window), menu_items_quark);
  const gchar* open_accel_path =
    gtk_menu_item_get_accel_path (GTK_MENU_ITEM (items->open_item));
  const gchar* quit_accel_path =
    gtk_menu_item_get_accel_path (GTK_MENU_ITEM (items->quit_item));

  if (gtk_widget_get_visible (items->edit_item))
    {
      gtk_widget_set_visible (items->edit_item, FALSE);
      gtk_accel_map_change_entry (open_accel_path, GDK_o,
                                  GDK_MOD1_MASK, TRUE);
      gtk_accel_map_change_entry (quit_accel_path, GDK_q,
                                  GDK_MOD1_MASK, TRUE);
    }
  else
    {
      gtk_widget_set_visible (items->edit_item, TRUE);
      gtk_accel_map_change_entry (open_accel_path, GDK_o,
                                  GDK_CONTROL_MASK, TRUE);
      gtk_accel_map_change_entry (quit_accel_path, GDK_q,
                                  GDK_CONTROL_MASK, TRUE);
    }
}

static void
view_menu_cb (GtkWidget *button, gpointer user_data)
{
#ifdef BUILT_UI
  GtkToggleButton *toggle = GTK_TOGGLE_BUTTON (button);
  static guint mergeid = 0;
  static GtkActionGroup* view_action_group = NULL;
  GtkUIManager *mgr = user_data;
  GtkWidget *window = gtk_widget_get_toplevel (button);
  GtkosxApplication *theApp = g_object_new (GTKOSX_TYPE_APPLICATION, NULL);
  GError *err = NULL;
  if (view_action_group == NULL)
    {
      view_action_group = gtk_action_group_new ("ViewAction");
      gtk_action_group_add_actions (view_action_group, view_menu,
                                    sizeof (view_menu) / sizeof (GtkActionEntry),
                                    NULL);
      gtk_action_group_add_radio_actions_full (
        view_action_group, view_actions,
        sizeof (view_actions) / sizeof (GtkRadioActionEntry),
        0, G_CALLBACK (radio_item_changed_cb),
        menu_cbdata_new ("View", GTK_WINDOW (window)),
        (GDestroyNotify) menu_cbdata_delete );
    }
  if (gtk_toggle_button_get_active (toggle))
    {
      mergeid = gtk_ui_manager_add_ui_from_file (mgr, "src/addedui.xml", &err);
      if (err)
        {
          g_print ("Error retrieving file: %d %s\n", mergeid, err->message);
        }
      gtk_ui_manager_insert_action_group (mgr, view_action_group, 0);
      {
        GtkWidget *menu = gtk_menu_new ();
        GtkWidget *item;
        item = gtk_menu_item_new_with_label ("Framish");
        g_signal_connect_data (item, "activate",
                               G_CALLBACK (menu_item_activate_cb),
                               menu_cbdata_new ( "Framish", item),
                               (GClosureNotify) menu_cbdata_delete, 0);
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
        item = gtk_menu_item_new_with_label ("Freebish");
        g_signal_connect_data (item, "activate",
                               G_CALLBACK (menu_item_activate_cb),
                               menu_cbdata_new ( "Freebish", item),
                               (GClosureNotify) menu_cbdata_delete, 0);
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
        gtkosx_application_set_dock_menu (theApp, GTK_MENU_SHELL (menu));
      }
    }
  else if (mergeid)
    {
      gtk_ui_manager_remove_action_group (mgr, view_action_group);
      gtk_ui_manager_remove_ui (mgr, mergeid);
      mergeid = 0;
    }
#else //Not BUILT_UI
  g_print ("View Menu Toggle Button doesn't actually do anything in the hand-built menu build\n");
#endif //BUILT_UI
}

#ifdef GTKOSXAPPLICATION
static void
app_active_cb (GtkosxApplication* app, gboolean* data)
{
  g_print ("Application became %s\n", *data ? "active" : "inactive");
}

static gboolean
app_should_quit_cb (GtkosxApplication *app, gpointer data)
{
  static gboolean abort = TRUE;
  gboolean answer;
  answer = abort;
  abort = FALSE;
  g_print ("Application has been requested to quit, %s\n", answer ? "but no!" :
           "it's OK.");
  return answer;
}

static void
app_will_quit_cb (GtkosxApplication *app, gpointer data)
{
  g_print ("Quitting Now\n");
  gtk_main_quit ();
}

static gboolean
app_open_file_cb (GtkosxApplication *app, gchar *path, gpointer user_data)
{
  g_print ("File open event for %s\n", path);
  return FALSE;
}

static gboolean
app_mm_key_cb (GtkosxApplication *app, int keyCode, gboolean isDown, gboolean isRepeat, gpointer user_data)
{
  g_print ("MM KEY EVENT for key %i\n", keyCode);
  if(keyCode == 16) return TRUE;
  else return FALSE;
}

#endif //GTKOSXAPPLICATION
#ifdef GTKMACINTEGRATION

gboolean _gtk_mac_menu_is_quit_menu_item_handled (void);

static GtkWidget *
create_window (GtkMacDock *dock, const gchar *title)
{
#else
static GtkWidget *
create_window (const gchar *title)
{
  gpointer	dock = NULL;
#endif //not GTKMACINTEGRATION
  GtkWidget	  *window;
  GtkWidget       *vbox;
  GtkWidget       *menubar;
  GtkWidget       *bbox;
  GtkWidget       *button;
  GtkWidget	  *textentry;
  MenuItems       *items = menu_items_new ();
#ifdef BUILT_UI
  GtkUIManager *mgr = gtk_ui_manager_new ();
  GtkActionGroup *actions = gtk_action_group_new ("TestActions");
  guint mergeid;
  GError *err = NULL;
  GtkAccelGroup *accel_group;
#else //not BUILT_UI
  GtkAccelGroup *accel_group = gtk_accel_group_new ();
#endif //not BUILT_UI
#ifdef GTKMACINTEGRATION
  GtkMacMenuGroup *group;
#endif //GTKMACINTEGRATION
#ifdef GTKOSXAPPLICATION
  GtkosxApplication *theApp = g_object_new (GTKOSX_TYPE_APPLICATION, NULL);
#endif //GTKOSXAPPLICATION
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  if (title)
    gtk_window_set_title (GTK_WINDOW (window), title);
  gtk_window_set_default_size (GTK_WINDOW (window), 400, 300);
  items->window = GTK_WINDOW (window);
#if GTK_CHECK_VERSION(3, 0, 0)
  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
#else
  vbox = gtk_vbox_new (FALSE, 0);
#endif
  gtk_container_add (GTK_CONTAINER (window), vbox);
#ifdef BUILT_UI
  mergeid = gtk_ui_manager_add_ui_from_file (mgr, "src/testui.xml", &err);
  if (err)
    {
      g_print ("Error retrieving file: %d %s\n", mergeid, err->message);
      exit (1);
    }
  gtk_action_group_add_actions (actions, test_actions,
                                G_N_ELEMENTS (test_actions),
                                (gpointer)window);
  gtk_ui_manager_insert_action_group (mgr, actions, 0);
  menubar = gtk_ui_manager_get_widget (mgr, "/menubar");
  items->open_item = gtk_ui_manager_get_widget (mgr, "/menubar/File/Open");
  items->edit_item = gtk_ui_manager_get_widget (mgr, "/menubar/Edit");
  items->copy_item = gtk_ui_manager_get_widget (mgr, "/menubar/Edit/Copy");
  items->help_menu = gtk_ui_manager_get_widget (mgr, "/menubar/Help");
  items->quit_item = gtk_ui_manager_get_widget (mgr, "/menubar/File/Quit");
  items->about_item = gtk_ui_manager_get_widget (mgr, "/menubar/Help/About");
  items->window_menu = gtk_ui_manager_get_widget (mgr, "/menubar/Window");
  items->preferences_item = gtk_ui_manager_get_widget (mgr, "/menubar/Edit/Preferences");
  accel_group = gtk_ui_manager_get_accel_group (mgr);
#else //not BUILT_UI
  menubar = test_setup_menu (items, accel_group);
  items->window_menu = NULL;
#endif //not BUILT_UI
  gtk_window_add_accel_group (GTK_WINDOW (window), accel_group);
  gtk_box_pack_start (GTK_BOX (vbox),
                      menubar,
                      FALSE, TRUE, 0);

  gtk_box_pack_start (GTK_BOX (vbox),
                      gtk_label_new ("Some window content here"),
                      FALSE, FALSE, 12);

#if GTK_CHECK_VERSION(3, 0, 0)
  bbox = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
#else
  bbox = gtk_hbutton_box_new ();
#endif //not Gtk3
  gtk_button_box_set_layout (GTK_BUTTON_BOX (bbox), GTK_BUTTONBOX_CENTER);
  gtk_box_set_spacing (GTK_BOX (bbox), 12);

  gtk_box_pack_start (GTK_BOX (vbox),
                      bbox,
                      TRUE, TRUE, 0);

  button = gtk_button_new_with_mnemonic ("Bo_unce");
  g_signal_connect (button, "clicked", G_CALLBACK (bounce_cb), dock);
  gtk_box_pack_start (GTK_BOX (bbox),
                      button,
                      FALSE, FALSE, 0);

  button = gtk_button_new_with_label ("Change Icon");
  g_signal_connect (button, "clicked", G_CALLBACK (change_icon_cb), dock);
  gtk_box_pack_start (GTK_BOX (bbox),
                      button,
                      FALSE, FALSE, 0);

  button = gtk_button_new_with_label ("Change Menu");
  g_signal_connect (button, "clicked", G_CALLBACK (change_menu_cb), NULL);
  gtk_box_pack_start (GTK_BOX (bbox),
                      button,
                      FALSE, FALSE, 0);
  button = gtk_toggle_button_new_with_label ("View Menu");
  textentry = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX (vbox), textentry, TRUE, TRUE, 2);
#ifdef BUILT_UI
  g_signal_connect (button, "toggled", G_CALLBACK (view_menu_cb), (gpointer)mgr);
#else //not BUILT_UI
  g_signal_connect (button, "toggled", G_CALLBACK (view_menu_cb), NULL);
#endif //not BUILT_UI
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), FALSE);
  gtk_box_pack_start (GTK_BOX (bbox),
                      button,
                      FALSE, FALSE, 0);

  button = gtk_toggle_button_new_with_label ("Sensitive_Prefs");
  g_signal_connect (button, "toggled", G_CALLBACK (toggle_prefs_cb), NULL);
  gtk_box_pack_start (GTK_BOX (bbox),
                      button,
                      FALSE, FALSE, 0);


  gtk_widget_show_all (window);
#if defined GTK_MAC_MENU || defined GTKOSXAPPLICATION
  gtk_widget_hide (menubar);
# ifdef GTKOSXAPPLICATION
  /* Not really necessary unless quartz accelerator handling is turned off. */
#  if !defined QUARTZ_HANDLERS && !defined BUILT_UI
  g_signal_connect (menubar, "can-activate-accel",
                    G_CALLBACK (can_activate_cb), NULL);
#  endif // !defined QUARTZ_HANDLERS && !defined BUILT_UI
# endif  //GTKOSXAPPLICATION
#endif //defined GTK_MAC_MENU || defined GTKOSXAPPLICATION
#ifdef GTKMACINTEGRATION
  gtk_mac_menu_set_menu_bar (GTK_MENU_SHELL (menubar));
  gtk_mac_menu_set_quit_menu_item (GTK_MENU_ITEM (items->quit_item));
  group = gtk_mac_menu_add_app_menu_group ();
  gtk_mac_menu_add_app_menu_item  (group,
                                   GTK_MENU_ITEM (items->about_item),
                                   "About");
  group = gtk_mac_menu_add_app_menu_group ();
  gtk_mac_menu_add_app_menu_item  (group,
                                   GTK_MENU_ITEM (items->preferences_item),
                                   "Preferences");
#endif //GTKMACINTEGRATION
#ifdef GTKOSXAPPLICATION
  GtkWidget *sep;
  gtkosx_application_set_menu_bar (theApp, GTK_MENU_SHELL (menubar));
  gtkosx_application_insert_app_menu_item  (theApp, items->about_item,
      0);
  sep = gtk_separator_menu_item_new ();
  g_object_ref (sep);
  gtkosx_application_insert_app_menu_item  (theApp, sep, 1);
  gtkosx_application_insert_app_menu_item  (theApp,
      items->preferences_item,
      2);
  sep = gtk_separator_menu_item_new ();
  g_object_ref (sep);
  gtkosx_application_insert_app_menu_item  (theApp, sep, 3);

  gtkosx_application_set_help_menu (theApp, GTK_MENU_ITEM (items->help_menu));
  gtkosx_application_set_window_menu (theApp, GTK_MENU_ITEM (items->window_menu));
#endif //GTKOSXAPPLICATION
  if (!menu_items_quark)
    menu_items_quark = g_quark_from_static_string ("MenuItem");
  g_object_set_qdata_full (G_OBJECT (window), menu_items_quark,
                           items, (GDestroyNotify)menu_items_destroy);
  return window;
}

int
main (int argc, char **argv)
{
  GtkWidget       *window1;
#ifndef BUILT_UI
  GtkWidget *window2;
#endif
#ifdef GTKMACINTEGRATION
  GtkMacDock      *dock;
#endif //GTKMACINTEGRATION
#ifdef GTKOSXAPPLICATION
  GtkosxApplication *theApp;
#endif //GTKOSXAPPLICATION
#if ! GLIB_CHECK_VERSION (2, 34, 0)
  g_thread_init (NULL);
#endif
#if ! GTK_CHECK_VERSION (3, 0, 0)
  gdk_threads_init ();
#endif //not Gtk3
  gtk_init (&argc, &argv);
#ifdef GTKMACINTEGRATION
  dock = gtk_mac_dock_get_default ();
  window1 = create_window (dock, "Test Integration Window 1");
  dock = gtk_mac_dock_new ();
  g_signal_connect (dock,
                    "clicked",
                    G_CALLBACK (dock_clicked_cb),
                    window1);
  g_signal_connect (dock,

                    "quit-activate",
                    G_CALLBACK (gtk_main_quit),
                    window1);
#elif defined(GTKOSXAPPLICATION)

  theApp  = g_object_new (GTKOSX_TYPE_APPLICATION, NULL);
  window1 = create_window ("Test Integration Window 1");
# ifndef BUILT_UI
  window2 = create_window ("Test Integration Window 2");
# endif
  {
    gboolean falseval = FALSE;
    gboolean trueval = TRUE;
    g_signal_connect (theApp, "NSApplicationDidBecomeActive",
                      G_CALLBACK (app_active_cb), &trueval);
    g_signal_connect (theApp, "NSApplicationWillResignActive",
                      G_CALLBACK (app_active_cb), &falseval);
    g_signal_connect (theApp, "NSApplicationBlockTermination",
                      G_CALLBACK (app_should_quit_cb), NULL);
    g_signal_connect (theApp, "NSApplicationWillTerminate",
                      G_CALLBACK (app_will_quit_cb), NULL);
    g_signal_connect (theApp, "NSApplicationOpenFile",
                      G_CALLBACK (app_open_file_cb), NULL);
    g_signal_connect (theApp, "MultiMediaKey",
                      G_CALLBACK (app_mm_key_cb), NULL);
  }
# ifndef QUARTZ_HANDLERS
  gtkosx_application_set_use_quartz_accelerators (theApp, FALSE);
# endif //QUARTZ_HANDLERS
  gtkosx_application_set_listen_for_multimedia_keys (theApp, TRUE);
  gtkosx_application_ready (theApp);
  {
    const gchar *id = gtkosx_application_get_bundle_id ();
    if (id != NULL)
      {
        g_print ("TestIntegration Error! Bundle Has ID %s\n", id);
      }
  }
#else //i.e., no integration
  window1 = create_window ("Test Integration Window 1");
  window2 = create_window ("Test Integration Window 2");
#endif //GTKOSXAPPLICATION
  gtk_accel_map_load ("accel_map");
  gtk_main ();
#ifdef GTKOSXAPPLICATION
  g_object_unref (theApp);
#endif //GTKOSXAPPLICATION
  return 0;
}
