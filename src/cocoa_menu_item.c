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

#import <Cocoa/Cocoa.h>
#include <gtk/gtk.h>
#if GTK_CHECK_VERSION(2,90,7)
#include <gdk/gdkkeysyms-compat.h>
#else
#include <gdk/gdkkeysyms.h>
#endif

#include "cocoa_menu_item.h"
#include "cocoa_menu.h"
#include "getlabel.h"
#include "gtk-mac-image-utils.h"
#import "GNSMenuBar.h"

//#define DEBUG(format, ...) g_printerr ("%s: " format, G_STRFUNC, ## __VA_ARGS__)
#define DEBUG(format, ...)

/*
 * These functions are long, ugly, and monotonous, so forward declare
 * them here and define them at the end of the file.
 */
static guint gdk_quartz_keyval_to_ns_keyval (guint keyval);
static gboolean keyval_is_keypad (guint keyval);
static guint keyval_keypad_nonkeypad_equivalent (guint keyval);
static const gchar* gdk_quartz_keyval_to_string (guint keyval);
static gboolean keyval_is_uppercase (guint keyval);

static GQuark cocoa_menu_item_quark = 0;

/*
 * utility functions
 */

static gboolean
accel_find_func (GtkAccelKey *key,
		 GClosure    *closure,
		 gpointer     data)
{
  return (GClosure *) data == closure;
}

static GClosure *
_gtk_accel_label_get_closure (GtkAccelLabel *label)
{
  g_return_val_if_fail(GTK_IS_ACCEL_LABEL(label), NULL);

  GClosure *closure = NULL;
  g_object_get(G_OBJECT(label), "accel-closure", &closure, NULL);
  return closure;
}

static void
cocoa_menu_item_free (gpointer *ptr)
{
  _GNSMenuItem* item = (_GNSMenuItem*) ptr;
  [item release];
}

_GNSMenuItem *
cocoa_menu_item_get (GtkWidget *widget)
{
  return (_GNSMenuItem*) g_object_get_qdata (G_OBJECT (widget), cocoa_menu_item_quark);
}

static void
cocoa_menu_item_update_state (_GNSMenuItem* cocoa_item,
			      GtkWidget      *widget)
{
  gboolean sensitive;
  gboolean visible;

  g_object_get (widget,
		"sensitive", &sensitive,
		"visible",   &visible,
		NULL);

  if (!sensitive)
    [cocoa_item setEnabled:NO];
  else
    [cocoa_item setEnabled:YES];

  if (!visible)
    [cocoa_item setHidden:YES];
  else
    [cocoa_item setHidden:NO];
}

static void
cocoa_menu_item_update_checked (_GNSMenuItem *cocoa_item,
			       GtkWidget  *widget)
{
  gboolean active, inconsistent;

  g_object_get (widget,
		"active", &active,
		"inconsistent", &inconsistent,
		NULL);

  if (inconsistent)
    [cocoa_item setState:NSMixedState];
  else if (active)
    [cocoa_item setState:NSOnState];
  else
    [cocoa_item setState:NSOffState];
}

static void
cocoa_menu_item_update_submenu (_GNSMenuItem *cocoa_item,
				GtkWidget      *widget)
{
  GtkWidget *submenu;

  g_return_if_fail (cocoa_item != NULL);
  g_return_if_fail (widget != NULL);

  submenu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (widget));

  if (!submenu) {
    if ([cocoa_item hasSubmenu])
    /*If the cocoa_item has a submenu but the menu_item doesn't,
      lose the cocoa_item's submenu */
      [cocoa_item setSubmenu: nil];
    return;
  }
  GtkWidget* label = NULL;
  NSMenu* cocoa_submenu = cocoa_menu_get(submenu);

  if (cocoa_submenu != nil)
    if ([cocoa_item submenu] != cocoa_submenu)
      //covers no submenu or wrong submenu on cocoa_item)
      [cocoa_item setSubmenu:cocoa_submenu];
    else ;
  //Nothing required -- the submenus are already set up correctly
  else if ([cocoa_item hasSubmenu]) {
    cocoa_submenu = [cocoa_item submenu];
    cocoa_menu_connect (submenu, cocoa_submenu);
  }
  else { //no submenu anywhere, so create one
    const gchar *text = get_menu_label_text (widget, &label);
    NSString *title = [NSString stringWithUTF8String: text ? text : ""];

    cocoa_submenu = [[[NSMenu alloc] initWithTitle: title] autorelease];

    [cocoa_submenu setAutoenablesItems:NO];
    cocoa_menu_connect (submenu, cocoa_submenu);

    /* connect the new nsmenu to the passed-in item (which lives in
       the parent nsmenu)
       (Note: this will release any pre-existing version of this submenu)
    */
    [ cocoa_item setSubmenu:cocoa_submenu];
  }
  /* and push the GTK menu into the submenu */
  cocoa_menu_item_add_submenu (GTK_MENU_SHELL (submenu), cocoa_submenu,
			       FALSE, FALSE);
}

static void
cocoa_menu_item_update_label (_GNSMenuItem *cocoa_item,
			      GtkWidget      *widget)
{
  g_return_if_fail (cocoa_item != NULL);
  g_return_if_fail (widget != NULL);

  {
    const gchar *text = get_menu_label_text (widget, NULL);
    NSString *title = [NSString stringWithUTF8String: text ? text : ""];
    [cocoa_item setTitle: title];
  }
}

static void
cocoa_menu_item_update_accelerator (_GNSMenuItem *cocoa_item,
				    GtkWidget *widget)
{
  GtkWidget *label;

  g_return_if_fail (cocoa_item != NULL);
  g_return_if_fail (widget != NULL);

  /* Important note: this function doesn't do anything to actually
   * change key handling. Its goal is to get Cocoa to display the
   * correct accelerator as part of a menu item. Actual accelerator
   * handling depends on gtkosx_application_use_quartz_accelerators,
   * so this is more cosmetic than it may appear.
  */
  /* Return if there's no label; it's probably a separator which isn't
   * going to get an accelerator anyway. */
  get_menu_label_text (widget, &label);
  if (label == NULL) return;

  GClosure *closure = NULL;
  g_object_get(label, "accel-closure", &closure, NULL);
  if (GTK_IS_ACCEL_LABEL (label) && closure)
    {
      GtkAccelKey *key;

      key = gtk_accel_group_find (gtk_accel_group_from_accel_closure(closure),
				  accel_find_func,
				  closure);

      if (key            &&
	  key->accel_key &&
	  key->accel_flags & GTK_ACCEL_VISIBLE)
	{
	  guint modifiers = 0;
	  const gchar* str = NULL;
	  guint actual_key = key->accel_key;

	  if (keyval_is_keypad (actual_key)) {
	    if ((actual_key = keyval_keypad_nonkeypad_equivalent (actual_key)) == GDK_VoidSymbol) {
	      /* GDK_KP_Separator */
	      [cocoa_item setKeyEquivalent:@""];
	      return;
	    }
	    modifiers |= NSNumericPadKeyMask;
	  }

	  /* if we somehow got here with GDK_A ... GDK_Z rather than GDK_a ... GDK_z, then take note
	     of that and make sure we use a shift modifier.
	  */

	  if (keyval_is_uppercase (actual_key)) {
	    modifiers |= NSShiftKeyMask;
	  }

	  str = gdk_quartz_keyval_to_string (actual_key);
	  if (str) {
	    unichar ukey = str[0];
	    [cocoa_item setKeyEquivalent:[NSString stringWithCharacters:&ukey length:1]];
	  } else {
	    unichar ukey = gdk_quartz_keyval_to_ns_keyval (actual_key);
	    if (ukey != 0) {
	      [cocoa_item setKeyEquivalent:[NSString stringWithCharacters:&ukey length:1]];
	    } else {
	      /* cannot map this key to Cocoa key equivalent */
	      [cocoa_item setKeyEquivalent:@""];
	      return;
	    }
	  }

	  if (key->accel_mods || modifiers)
	    {
	      if (key->accel_mods & GDK_SHIFT_MASK) {
		modifiers |= NSShiftKeyMask;
	      }

	      if (key->accel_mods & GDK_CONTROL_MASK) {
		modifiers |= NSControlKeyMask;
	      }

	      /* gdk/quartz maps Alt/Option to Mod5 */
	      if (key->accel_mods & (GDK_MOD1_MASK)) {
		modifiers |= NSAlternateKeyMask;
	      }

	      /* gdk/quartz maps Command to MOD1 */
	      if (key->accel_mods & GDK_META_MASK) {
		modifiers |= NSCommandKeyMask;
	      }

	    }

	  [cocoa_item setKeyEquivalentModifierMask:modifiers];
	  return;
	}
    }

  /*  otherwise, clear the menu shortcut  */
  [cocoa_item setKeyEquivalent:@""];
}

static void
cocoa_menu_item_accel_changed (GtkAccelGroup   *accel_group,
			       guint            keyval,
			       GdkModifierType  modifier,
			       GClosure        *accel_closure,
			       GtkWidget       *widget)
{
  _GNSMenuItem *cocoa_item = cocoa_menu_item_get (widget);
  GtkWidget      *label;

  get_menu_label_text (widget, &label);
  if (gtk_accel_group_from_accel_closure(accel_closure) != accel_group)
      return;
  if (GTK_IS_ACCEL_LABEL (label) &&
      _gtk_accel_label_get_closure((GtkAccelLabel *) label) == accel_closure)
    cocoa_menu_item_update_accelerator (cocoa_item, widget);
}

static void
cocoa_menu_item_update_accel_closure (_GNSMenuItem *cocoa_item,
				      GtkWidget      *widget)
{
  GtkAccelGroup *group;
  GtkWidget     *label;

  get_menu_label_text (widget, &label);

  if (cocoa_item->accel_closure)
    {
      group = gtk_accel_group_from_accel_closure (cocoa_item->accel_closure);

      g_signal_handlers_disconnect_by_func (group,
					    (void*) cocoa_menu_item_accel_changed,
					    widget);

      g_closure_unref (cocoa_item->accel_closure);
      cocoa_item->accel_closure = NULL;
    }

  if (GTK_IS_ACCEL_LABEL (label)) {
	cocoa_item->accel_closure = _gtk_accel_label_get_closure((GtkAccelLabel *) label);
  }

  if (cocoa_item->accel_closure)
    {
      g_closure_ref (cocoa_item->accel_closure);

      group = gtk_accel_group_from_accel_closure (cocoa_item->accel_closure);

      g_signal_connect_object (group, "accel-changed",
			       G_CALLBACK (cocoa_menu_item_accel_changed),
			       widget, (GConnectFlags) 0);
    }

  cocoa_menu_item_update_accelerator (cocoa_item, widget);
}

static void
cocoa_menu_item_notify_label (GObject    *object,
			      GParamSpec *pspec,
			      gpointer    data)
{
  _GNSMenuItem *cocoa_item = cocoa_menu_item_get (GTK_WIDGET (object));

  if (!strcmp (pspec->name, "label"))
    {
      cocoa_menu_item_update_label (cocoa_item,
				    GTK_WIDGET (object));
    }
  else if (!strcmp (pspec->name, "accel-closure"))
    {
      cocoa_menu_item_update_accel_closure (cocoa_item,
					    GTK_WIDGET (object));
    }
}

static void
cocoa_menu_item_notify (GObject        *object,
			GParamSpec     *pspec,
			_GNSMenuItem *cocoa_item)
{
  if (!strcmp (pspec->name, "sensitive") ||
      !strcmp (pspec->name, "visible"))
    {
      cocoa_menu_item_update_state (cocoa_item, GTK_WIDGET (object));
    }
  else if (!strcmp (pspec->name, "active") ||
	   !strcmp (pspec->name, "inconsistent"))
    {
      cocoa_menu_item_update_checked (cocoa_item, GTK_WIDGET (object));
    }
  else if (!strcmp (pspec->name, "submenu"))
    {
      cocoa_menu_item_update_submenu (cocoa_item, GTK_WIDGET (object));
    }
}

static void
cocoa_menu_item_connect (GtkWidget*   menu_item,
			 _GNSMenuItem* cocoa_item,
			 GtkWidget     *label)
{
  _GNSMenuItem* old_item = cocoa_menu_item_get (menu_item);

  if (old_item == cocoa_item)
      return;

  [cocoa_item retain];

  if (cocoa_menu_item_quark == 0)
    cocoa_menu_item_quark = g_quark_from_static_string ("_GNSMenuItem");

  g_object_set_qdata_full (G_OBJECT (menu_item), cocoa_menu_item_quark,
			   cocoa_item,
			   (GDestroyNotify) cocoa_menu_item_free);

  if (old_item) {
      GSignalMatchType mask = G_SIGNAL_MATCH_ID | G_SIGNAL_MATCH_DATA;
      guint notify_sig = g_signal_lookup("notify", GTK_TYPE_MENU_ITEM);
      if (! g_signal_handlers_disconnect_matched(G_OBJECT(menu_item),
						 mask, notify_sig,
						 (GQuark)0, NULL, NULL,
						 old_item))
	  g_print ("Failed to disconnect old notify signal for %s\\n",
		   gtk_widget_get_name(menu_item));
      [old_item release];
  }
  g_signal_connect (menu_item, "notify",
		    G_CALLBACK (cocoa_menu_item_notify),
		    cocoa_item);

  if (label)
      g_signal_connect_swapped (label, "notify::label",
				G_CALLBACK (cocoa_menu_item_notify_label),
				menu_item);
}

static void
cocoa_menu_item_sync (GtkWidget* menu_item)
{
  _GNSMenuItem *cocoa_item = cocoa_menu_item_get (menu_item);
  cocoa_menu_item_update_state (cocoa_item, menu_item);

  if (GTK_IS_CHECK_MENU_ITEM (menu_item))
    cocoa_menu_item_update_checked (cocoa_item, menu_item);

  if (!GTK_IS_SEPARATOR_MENU_ITEM (menu_item))
    cocoa_menu_item_update_accel_closure (cocoa_item, menu_item);

  if (gtk_menu_item_get_submenu (GTK_MENU_ITEM (menu_item)))
    cocoa_menu_item_update_submenu (cocoa_item, menu_item);

}

/*
 * nsimage_from_pixbuf:
 * @pixbuf: The GdkPixbuf* to convert
 *
 * Create an NSImage from a CGImageRef.
 * Lifted from http://www.cocoadev.com/index.pl?CGImageRef
 *
 * Returns: An auto-released NSImage*
 */
static NSImage*
nsimage_from_pixbuf(GdkPixbuf *pixbuf)
{
  CGImageRef image = NULL;
  NSRect imageRect = NSMakeRect(0.0, 0.0, 0.0, 0.0);
  CGContextRef imageContext = nil;
  NSImage* newImage = nil;

  g_return_val_if_fail (pixbuf !=  NULL, NULL);
  image = gtkosx_create_cgimage_from_pixbuf (pixbuf);
  // Get the image dimensions.
  imageRect.size.height = CGImageGetHeight(image);
  imageRect.size.width = CGImageGetWidth(image);

  // Create a new image to receive the Quartz image data.
  newImage = [[[NSImage alloc] initWithSize:imageRect.size] autorelease];
  [newImage lockFocus];

  // Get the Quartz context and draw.
  imageContext = (CGContextRef)[[NSGraphicsContext currentContext]
                                graphicsPort];
  CGContextDrawImage(imageContext, *(CGRect*)&imageRect, image);
  [newImage unlockFocus];
  CGImageRelease (image);
  return newImage;
}

/*
 * If the menu item does contain an image Widget, depending of what this widget is,
 * get a PixBuf with the following method:
 *   - menu_item is GtkImage of type GTK_IMAGE_PIXBUF: use gtk_image_get_pixbuf()
 *   - menu_item is GtkImage of type GTK_IMAGE_STOCK: use gtk_widget_render_icon()
 *   - menu_item is GtkWidget: use gtk_offscreen_window_get_pixbuf() on GtkOffscreenWindow
 */
static void
cocoa_menu_item_add_item_image (_GNSMenuItem* cocoa_item, GtkWidget* menu_item)
{
  GtkWidget *menu_image = gtk_image_menu_item_get_image (GTK_IMAGE_MENU_ITEM(menu_item));

  if (menu_image) {
    NSImage *image = nil;
    if (GTK_IS_IMAGE (menu_image)) {
      if (gtk_image_get_storage_type(GTK_IMAGE(menu_image)) == GTK_IMAGE_PIXBUF) {
        DEBUG("Menu image is GTK_IMAGE_PIXBUF\n");
        GdkPixbuf *pixbuf = gtk_image_get_pixbuf(GTK_IMAGE(menu_image));
        image = nsimage_from_pixbuf(pixbuf);
      }
      else if (gtk_image_get_storage_type(GTK_IMAGE(menu_image)) == GTK_IMAGE_STOCK) {
        DEBUG("Menu image is GTK_IMAGE_STOCK\n");
        gchar *stock_id;
        GtkIconSize size;
        gtk_image_get_stock(GTK_IMAGE(menu_image), &stock_id, &size);
        GdkPixbuf *pixbuf = gtk_widget_render_icon(menu_image, stock_id, size, "");
        image = nsimage_from_pixbuf(pixbuf);
        g_object_unref(pixbuf);
      }
    }
    if (!image) {
      DEBUG("Menu image is GTK_WIDGET\n");
      // Take menu image widget out of menu item
      g_object_ref(menu_image);
      gtk_widget_unparent (menu_image);
      GTK_IMAGE_MENU_ITEM(menu_item)->image = NULL;
      g_object_notify(G_OBJECT(menu_item), "image");
      // Put menu image widget in Offscreen Window, take a pixbuf
      GtkWidget *offscreenWindow = gtk_offscreen_window_new();
      gtk_container_add(GTK_CONTAINER(offscreenWindow), menu_image);
      // Show with proper size
      gtk_widget_show_all(offscreenWindow);
      GtkRequisition req;
      GtkAllocation alloc;
      gtk_widget_size_request (menu_image, &req);
      alloc.x = alloc.y = 0;
      alloc.width = req.width;
      alloc.height = req.height;
      gtk_widget_size_allocate (menu_image, &alloc);
      while (gtk_events_pending ())
        gtk_main_iteration_do (FALSE);
      // Take a pixbuf of the offscreen window, convert to image
      GdkPixbuf *pixbuf = gtk_offscreen_window_get_pixbuf(GTK_OFFSCREEN_WINDOW(offscreenWindow));
      image = nsimage_from_pixbuf(pixbuf);
      g_object_unref(pixbuf);
      // Take menu image widget out of offscreen window, back into menu item
      gtk_container_remove(GTK_CONTAINER(offscreenWindow), menu_image);
      gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menu_item), menu_image);
      g_object_unref(menu_image);
    }
    [cocoa_item setImage: image];
  }

}

/*
 * Public Functions
 */

void
cocoa_menu_item_add_item (NSMenu* cocoa_menu, GtkWidget* menu_item, int index)
{
  GtkWidget* label      = NULL;
  _GNSMenuItem *cocoa_item;

  DEBUG ("add %s to menu %s separator ? %d\n",
	 get_menu_label_text (menu_item, NULL),
	 [[cocoa_menu title] cStringUsingEncoding:NSUTF8StringEncoding],
	 GTK_IS_SEPARATOR_MENU_ITEM(menu_item));

  cocoa_item = cocoa_menu_item_get (menu_item);

  if (cocoa_item) {
    DEBUG ("\tItem exists\n");
    [cocoa_item retain];
    [[cocoa_item menu] removeItem: cocoa_item];
  }

  if (GTK_IS_SEPARATOR_MENU_ITEM (menu_item)) {
    cocoa_item = (_GNSMenuItem*)[_GNSMenuItem separatorItem];
    DEBUG ("\ta separator\n");
  } else {
    const gchar* text = get_menu_label_text (menu_item, &label);
    NSString *title = [NSString stringWithUTF8String:(text ? text : @"")];

    GClosure *menu_action =
      g_cclosure_new_object_swap(G_CALLBACK(gtk_menu_item_activate),
				 G_OBJECT(menu_item));
    g_closure_set_marshal(menu_action, g_cclosure_marshal_VOID__VOID);

    cocoa_item = [[_GNSMenuItem alloc]
                   initWithTitle: title
                   aGClosure:menu_action andPointer:NULL];
    
    DEBUG ("\tan item\n");
  }
  cocoa_menu_item_connect (menu_item, (_GNSMenuItem*) cocoa_item, label);
  if (GTK_IS_IMAGE_MENU_ITEM (menu_item)) {
    cocoa_menu_item_add_item_image(cocoa_item, menu_item);
  }
  [ cocoa_item setEnabled:YES];

  /* connect GtkMenuItem and _GNSMenuItem so that we can notice changes
   * to accel/label/submenu etc. */

  if (index >= 0 && index < [cocoa_menu numberOfItems])
    [ cocoa_menu insertItem:cocoa_item atIndex:index];
  else
    [ cocoa_menu addItem:cocoa_item];

  cocoa_menu_item_sync(menu_item);
}

void
cocoa_menu_item_add_submenu (GtkMenuShell *menu_shell,
			   NSMenu*       cocoa_menu,
			   gboolean      toplevel,
			   gboolean      debug)
{
  GList         *children;
  GList         *l;
  guint index = 0, count, loc;

  count = [cocoa_menu numberOfItems];
  /* First go through the cocoa menu and mark all of the items unused. */
  for (index = 0; index < count; index++) {
    _GNSMenuItem *indexedItem = (_GNSMenuItem*)[cocoa_menu itemAtIndex: index];
    if (GTK_IS_MENU_BAR(menu_shell) &&
	(indexedItem == [(_GNSMenuBar*)cocoa_menu windowsMenu] ||
	 indexedItem == [(_GNSMenuBar*)cocoa_menu helpMenu] ||
	 indexedItem == [(_GNSMenuBar*)cocoa_menu appMenu]))
      continue;
    if ([[cocoa_menu itemAtIndex: index] respondsToSelector: @selector(mark)])
      [(_GNSMenuItem*)[cocoa_menu itemAtIndex: index] mark];
  }
  index = toplevel ? 1 : 0; //Skip the 0th menu item on the menu bar
  /* Now iterate over the menu shell and check it against the cocoa menu */
  children = gtk_container_get_children (GTK_CONTAINER (menu_shell));
  for (l = children; l; l = l->next) {
    GtkWidget   *menu_item = (GtkWidget*) l->data;
    _GNSMenuItem *cocoa_item =  cocoa_menu_item_get (menu_item);
    if ([cocoa_item menu] && [cocoa_item menu] != cocoa_menu)
      /* This item has been moved to another menu; skip it */
      continue;
    if ([cocoa_item respondsToSelector: @selector(isMarked)] &&
	[cocoa_menu numberOfItems] > index &&
	[cocoa_menu itemAtIndex:index] == cocoa_item) {
      /* This item is where it belongs, so unmark and update it */
      [cocoa_item unmark];
      cocoa_menu_item_sync(menu_item);
      ++index;
      continue;
    }
    if (cocoa_item && (loc = [cocoa_menu indexOfItem: cocoa_item]) > -1) {
      /*It's in there, just in the wrong place. Put it where it goes
	and update it*/
      [cocoa_item retain];
      [cocoa_menu removeItem: cocoa_item];
      [cocoa_menu insertItem: cocoa_item atIndex: index++];
      if ([cocoa_item respondsToSelector: @selector(isMarked)])
	[cocoa_item unmark];
      [cocoa_item release];
      cocoa_menu_item_sync(menu_item);
      continue;
    }
    if (GTK_IS_SEPARATOR_MENU_ITEM (menu_item) && GTK_IS_MENU_BAR(menu_shell))
      /* Don't want separators on the menubar */
      continue;

    if (GTK_IS_TEAROFF_MENU_ITEM (menu_item))
      /*Don't want tearoff items at all */
      continue;

    if (g_object_get_data (G_OBJECT (menu_item), "gtk-empty-menu-item"))
      /* Nor blank items. */
      continue;
    /*OK, this must be a new one. Add it. */
    cocoa_menu_item_add_item (cocoa_menu, menu_item, index++);
  }
  /* Iterate over the cocoa menu again removing anything that's still marked */
  for (index = 0; index < [cocoa_menu numberOfItems]; index++) {
    _GNSMenuItem *item = (_GNSMenuItem*)[cocoa_menu itemAtIndex: index];
    if (([item respondsToSelector: @selector(isMarked)]) && [item isMarked])
      [cocoa_menu removeItem: item];
  }

  g_list_free (children);
}

/*
 * The Keyval functions, forward declared at the top of the file.
 */

static guint
gdk_quartz_keyval_to_ns_keyval (guint keyval)
{
  switch (keyval) {
  case GDK_BackSpace:
    return NSBackspaceCharacter;
  case GDK_Delete:
    return NSDeleteFunctionKey;
  case GDK_Pause:
    return NSPauseFunctionKey;
  case GDK_Scroll_Lock:
    return NSScrollLockFunctionKey;
  case GDK_Sys_Req:
    return NSSysReqFunctionKey;
  case GDK_Home:
    return NSHomeFunctionKey;
  case GDK_Left:
  case GDK_leftarrow:
    return NSLeftArrowFunctionKey;
  case GDK_Up:
  case GDK_uparrow:
    return NSUpArrowFunctionKey;
  case GDK_Right:
  case GDK_rightarrow:
    return NSRightArrowFunctionKey;
  case GDK_Down:
  case GDK_downarrow:
    return NSDownArrowFunctionKey;
  case GDK_Page_Up:
    return NSPageUpFunctionKey;
  case GDK_Page_Down:
    return NSPageDownFunctionKey;
  case GDK_End:
    return NSEndFunctionKey;
  case GDK_Begin:
    return NSBeginFunctionKey;
  case GDK_Select:
    return NSSelectFunctionKey;
  case GDK_Print:
    return NSPrintFunctionKey;
  case GDK_Execute:
    return NSExecuteFunctionKey;
  case GDK_Insert:
    return NSInsertFunctionKey;
  case GDK_Undo:
    return NSUndoFunctionKey;
  case GDK_Redo:
    return NSRedoFunctionKey;
  case GDK_Menu:
    return NSMenuFunctionKey;
  case GDK_Find:
    return NSFindFunctionKey;
  case GDK_Help:
    return NSHelpFunctionKey;
  case GDK_Break:
    return NSBreakFunctionKey;
  case GDK_Mode_switch:
    return NSModeSwitchFunctionKey;
  case GDK_F1:
    return NSF1FunctionKey;
  case GDK_F2:
    return NSF2FunctionKey;
  case GDK_F3:
    return NSF3FunctionKey;
  case GDK_F4:
    return NSF4FunctionKey;
  case GDK_F5:
    return NSF5FunctionKey;
  case GDK_F6:
    return NSF6FunctionKey;
  case GDK_F7:
    return NSF7FunctionKey;
  case GDK_F8:
    return NSF8FunctionKey;
  case GDK_F9:
    return NSF9FunctionKey;
  case GDK_F10:
    return NSF10FunctionKey;
  case GDK_F11:
    return NSF11FunctionKey;
  case GDK_F12:
    return NSF12FunctionKey;
  case GDK_F13:
    return NSF13FunctionKey;
  case GDK_F14:
    return NSF14FunctionKey;
  case GDK_F15:
    return NSF15FunctionKey;
  case GDK_F16:
    return NSF16FunctionKey;
  case GDK_F17:
    return NSF17FunctionKey;
  case GDK_F18:
    return NSF18FunctionKey;
  case GDK_F19:
    return NSF19FunctionKey;
  case GDK_F20:
    return NSF20FunctionKey;
  case GDK_F21:
    return NSF21FunctionKey;
  case GDK_F22:
    return NSF22FunctionKey;
  case GDK_F23:
    return NSF23FunctionKey;
  case GDK_F24:
    return NSF24FunctionKey;
  case GDK_F25:
    return NSF25FunctionKey;
  case GDK_F26:
    return NSF26FunctionKey;
  case GDK_F27:
    return NSF27FunctionKey;
  case GDK_F28:
    return NSF28FunctionKey;
  case GDK_F29:
    return NSF29FunctionKey;
  case GDK_F30:
    return NSF30FunctionKey;
  case GDK_F31:
    return NSF31FunctionKey;
  case GDK_F32:
    return NSF32FunctionKey;
  case GDK_F33:
    return NSF33FunctionKey;
  case GDK_F34:
    return NSF34FunctionKey;
  case GDK_F35:
    return NSF35FunctionKey;
  default:
    break;
  }

  return 0;
}

static gboolean
keyval_is_keypad (guint keyval)
{
  switch (keyval) {
  case GDK_KP_F1:
  case GDK_KP_F2:
  case GDK_KP_F3:
  case GDK_KP_F4:
  case GDK_KP_Home:
  case GDK_KP_Left:
  case GDK_KP_Up:
  case GDK_KP_Right:
  case GDK_KP_Down:
  case GDK_KP_Page_Up:
  case GDK_KP_Page_Down:
  case GDK_KP_End:
  case GDK_KP_Begin:
  case GDK_KP_Insert:
  case GDK_KP_Delete:
  case GDK_KP_Equal:
  case GDK_KP_Multiply:
  case GDK_KP_Add:
  case GDK_KP_Separator:
  case GDK_KP_Subtract:
  case GDK_KP_Decimal:
  case GDK_KP_Divide:
  case GDK_KP_0:
  case GDK_KP_1:
  case GDK_KP_2:
  case GDK_KP_3:
  case GDK_KP_4:
  case GDK_KP_5:
  case GDK_KP_6:
  case GDK_KP_7:
  case GDK_KP_8:
  case GDK_KP_9:
    return TRUE;
    break;
  default:
    break;
  }
  return FALSE;
}

static guint
keyval_keypad_nonkeypad_equivalent (guint keyval)
{
  switch (keyval) {
  case GDK_KP_F1:
    return GDK_F1;
  case GDK_KP_F2:
    return GDK_F2;
  case GDK_KP_F3:
    return GDK_F3;
  case GDK_KP_F4:
    return GDK_F4;
  case GDK_KP_Home:
    return GDK_Home;
  case GDK_KP_Left:
    return GDK_Left;
  case GDK_KP_Up:
    return GDK_Up;
  case GDK_KP_Right:
    return GDK_Right;
  case GDK_KP_Down:
    return GDK_Down;
  case GDK_KP_Page_Up:
    return GDK_Page_Up;
  case GDK_KP_Page_Down:
    return GDK_Page_Down;
  case GDK_KP_End:
    return GDK_End;
  case GDK_KP_Begin:
    return GDK_Begin;
  case GDK_KP_Insert:
    return GDK_Insert;
  case GDK_KP_Delete:
    return GDK_Delete;
  case GDK_KP_Equal:
    return GDK_equal;
  case GDK_KP_Multiply:
    return GDK_asterisk;
  case GDK_KP_Add:
    return GDK_plus;
  case GDK_KP_Subtract:
    return GDK_minus;
  case GDK_KP_Decimal:
    return GDK_period;
  case GDK_KP_Divide:
    return GDK_slash;
  case GDK_KP_0:
    return GDK_0;
  case GDK_KP_1:
    return GDK_1;
  case GDK_KP_2:
    return GDK_2;
  case GDK_KP_3:
    return GDK_3;
  case GDK_KP_4:
    return GDK_4;
  case GDK_KP_5:
    return GDK_5;
  case GDK_KP_6:
    return GDK_6;
  case GDK_KP_7:
    return GDK_7;
  case GDK_KP_8:
    return GDK_8;
  case GDK_KP_9:
    return GDK_9;
  default:
    break;
  }

  return GDK_VoidSymbol;
}

static const gchar*
gdk_quartz_keyval_to_string (guint keyval)
{
  switch (keyval) {
  case GDK_space:
    return " ";
  case GDK_exclam:
    return "!";
  case GDK_quotedbl:
    return "\"";
  case GDK_numbersign:
    return "#";
  case GDK_dollar:
    return "$";
  case GDK_percent:
    return "%";
  case GDK_ampersand:
    return "&";
  case GDK_apostrophe:
    return "'";
  case GDK_parenleft:
    return "(";
  case GDK_parenright:
    return ")";
  case GDK_asterisk:
    return "*";
  case GDK_plus:
    return "+";
  case GDK_comma:
    return ",";
  case GDK_minus:
    return "-";
  case GDK_period:
    return ".";
  case GDK_slash:
    return "/";
  case GDK_0:
    return "0";
  case GDK_1:
    return "1";
  case GDK_2:
    return "2";
  case GDK_3:
    return "3";
  case GDK_4:
    return "4";
  case GDK_5:
    return "5";
  case GDK_6:
    return "6";
  case GDK_7:
    return "7";
  case GDK_8:
    return "8";
  case GDK_9:
    return "9";
  case GDK_colon:
    return ":";
  case GDK_semicolon:
    return ";";
  case GDK_less:
    return "<";
  case GDK_equal:
    return "=";
  case GDK_greater:
    return ">";
  case GDK_question:
    return "?";
  case GDK_at:
    return "@";
  case GDK_A:
  case GDK_a:
    return "a";
  case GDK_B:
  case GDK_b:
    return "b";
  case GDK_C:
  case GDK_c:
    return "c";
  case GDK_D:
  case GDK_d:
    return "d";
  case GDK_E:
  case GDK_e:
    return "e";
  case GDK_F:
  case GDK_f:
    return "f";
  case GDK_G:
  case GDK_g:
    return "g";
  case GDK_H:
  case GDK_h:
    return "h";
  case GDK_I:
  case GDK_i:
    return "i";
  case GDK_J:
  case GDK_j:
    return "j";
  case GDK_K:
  case GDK_k:
    return "k";
  case GDK_L:
  case GDK_l:
    return "l";
  case GDK_M:
  case GDK_m:
    return "m";
  case GDK_N:
  case GDK_n:
    return "n";
  case GDK_O:
  case GDK_o:
    return "o";
  case GDK_P:
  case GDK_p:
    return "p";
  case GDK_Q:
  case GDK_q:
    return "q";
  case GDK_R:
  case GDK_r:
    return "r";
  case GDK_S:
  case GDK_s:
    return "s";
  case GDK_T:
  case GDK_t:
    return "t";
  case GDK_U:
  case GDK_u:
    return "u";
  case GDK_V:
  case GDK_v:
    return "v";
  case GDK_W:
  case GDK_w:
    return "w";
  case GDK_X:
  case GDK_x:
    return "x";
  case GDK_Y:
  case GDK_y:
    return "y";
  case GDK_Z:
  case GDK_z:
    return "z";
  case GDK_bracketleft:
    return "[";
  case GDK_backslash:
    return "\\";
  case GDK_bracketright:
    return "]";
  case GDK_asciicircum:
    return "^";
  case GDK_underscore:
    return "_";
  case GDK_grave:
    return "`";
  case GDK_braceleft:
    return "{";
  case GDK_bar:
    return "|";
  case GDK_braceright:
    return "}";
  case GDK_asciitilde:
    return "~";
  default:
    break;
  }
  return NULL;
};
static gboolean
keyval_is_uppercase (guint keyval)
{
  switch (keyval) {
  case GDK_A:
  case GDK_B:
  case GDK_C:
  case GDK_D:
  case GDK_E:
  case GDK_F:
  case GDK_G:
  case GDK_H:
  case GDK_I:
  case GDK_J:
  case GDK_K:
  case GDK_L:
  case GDK_M:
  case GDK_N:
  case GDK_O:
  case GDK_P:
  case GDK_Q:
  case GDK_R:
  case GDK_S:
  case GDK_T:
  case GDK_U:
  case GDK_V:
  case GDK_W:
  case GDK_X:
  case GDK_Y:
  case GDK_Z:
    return TRUE;
  default:
    return FALSE;
  }
  return FALSE;
}
