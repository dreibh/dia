/* Dia -- an diagram creation/manipulation program
 * Copyright (C) 2003 Lars Clausen
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/* persistence.c -- functions that handle persistent stores, such as 
 * window positions and sizes, font menu, document history etc.
 */

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "persistence.h"
#include "dia_dirs.h"
#include "dia_xml_libxml.h"
#include "dia_xml.h"

#include <gtk/gtk.h>
#include <libxml/tree.h>

/* Hash table from window role (string) to PersistentWindow structure.
 */
static GHashTable *persistent_windows, *persistent_entrystrings, *persistent_lists;
static GHashTable *persistent_integers, *persistent_reals;
static GHashTable *persistent_booleans, *persistent_strings;
static GHashTable *persistent_colors;

/* *********************** LOADING FUNCTIONS *********************** */

typedef void (*PersistenceLoadFunc)(gchar *role, xmlNodePtr node);

static void
persistence_load_window(gchar *role, xmlNodePtr node)
{
  AttributeNode attr;
  PersistentWindow *wininfo = g_new0(PersistentWindow, 1);

  attr = composite_find_attribute(node, "xpos");
  if (attr != NULL)
    wininfo->x = data_int(attribute_first_data(attr));
  attr = composite_find_attribute(node, "ypos");
  if (attr != NULL)
    wininfo->y = data_int(attribute_first_data(attr));
  attr = composite_find_attribute(node, "width");
  if (attr != NULL)
    wininfo->width = data_int(attribute_first_data(attr));
  attr = composite_find_attribute(node, "height");
  if (attr != NULL)
    wininfo->height = data_int(attribute_first_data(attr));
  attr = composite_find_attribute(node, "isopen");
  if (attr != NULL)
    wininfo->isopen = data_boolean(attribute_first_data(attr));

  g_hash_table_insert(persistent_windows, role, wininfo);
}

/** Load a persistent string into the strings hashtable */
static void
persistence_load_entrystring(gchar *role, xmlNodePtr node)
{
  AttributeNode attr;
  gchar *string = NULL;

  /* Find the contents? */
  attr = composite_find_attribute(node, "stringvalue");
  if (attr != NULL)
    string = data_string(attribute_first_data(attr));
  else 
    return;

  if (string != NULL)
    g_hash_table_insert(persistent_entrystrings, role, string);
}

static void
persistence_load_list(gchar *role, xmlNodePtr node)
{
  AttributeNode attr;
  gchar *string = NULL;

  /* Find the contents? */
  attr = composite_find_attribute(node, "listvalue");
  if (attr != NULL)
    string = data_string(attribute_first_data(attr));
  else 
    return;

  if (string != NULL) {
    gchar **strings = g_strsplit(string, "\n", -1);
    PersistentList *plist;
    GList *list = NULL;
    int i;
    for (i = 0; strings[i] != NULL; i++) {
      list = g_list_append(list, g_strdup(strings[i]));
    }
    /* This frees the strings, too? */
    g_strfreev(strings);
    plist = g_new(PersistentList, 1);
    plist->glist = list;
    plist->role = role;
    plist->sorted = FALSE;
    plist->max_members = G_MAXINT;
    g_hash_table_insert(persistent_lists, role, plist);
  }
}

static void
persistence_load_integer(gchar *role, xmlNodePtr node)
{
  AttributeNode attr;
  gint *integer;

  /* Find the contents? */
  attr = composite_find_attribute(node, "intvalue");
  if (attr != NULL) {
    integer = g_new(gint, 1);
    *integer = data_int(attribute_first_data(attr));
  } else 
    return;

  if (g_hash_table_lookup(persistent_integers, role) == NULL) 
    g_hash_table_insert(persistent_integers, role, integer);
  else 
    printf("Int %s registered before loading persistence!\n", role);
}

static void
persistence_load_real(gchar *role, xmlNodePtr node)
{
  AttributeNode attr;
  real *realval;

  /* Find the contents? */
  attr = composite_find_attribute(node, "realvalue");
  if (attr != NULL) {
    realval = g_new(real, 1);
    *realval = data_real(attribute_first_data(attr));
  } else 
    return;

  if (g_hash_table_lookup(persistent_reals, role) == NULL) 
    g_hash_table_insert(persistent_reals, role, realval);
  else 
    printf("Real %s registered before loading persistence!\n", role);
}

static void
persistence_load_boolean(gchar *role, xmlNodePtr node)
{
  AttributeNode attr;
  gboolean *booleanval;

  /* Find the contents? */
  attr = composite_find_attribute(node, "booleanvalue");
  if (attr != NULL) {
    booleanval = g_new(gboolean, 1);
    *booleanval = data_boolean(attribute_first_data(attr));
  } else 
    return;

  if (g_hash_table_lookup(persistent_booleans, role) == NULL) 
    g_hash_table_insert(persistent_booleans, role, booleanval);
  else 
    printf("Boolean %s registered before loading persistence!\n", role);
}

static void
persistence_load_string(gchar *role, xmlNodePtr node)
{
  AttributeNode attr;
  gchar *stringval;

  /* Find the contents? */
  attr = composite_find_attribute(node, "stringvalue");
  if (attr != NULL) {
    stringval = data_string(attribute_first_data(attr));
  } else 
    return;

  if (g_hash_table_lookup(persistent_strings, role) == NULL) 
    g_hash_table_insert(persistent_strings, role, stringval);
  else 
    printf("String %s registered before loading persistence!\n", role);
}

static void
persistence_load_color(gchar *role, xmlNodePtr node)
{
  AttributeNode attr;
  Color *colorval;

  /* Find the contents? */
  attr = composite_find_attribute(node, "colorvalue");
  if (attr != NULL) {
    colorval = g_new(Color, 1);
    data_color(attribute_first_data(attr), colorval);
  } else 
    return;

  if (g_hash_table_lookup(persistent_colors, role) == NULL) 
    g_hash_table_insert(persistent_colors, role, colorval);
  else 
    printf("Color %s registered before loading persistence!\n", role);
}

static xmlNodePtr
find_node_named (xmlNodePtr p, const char *name)
{
  while (p && 0 != strcmp(p->name, name))
    p = p->next;
  return p;
}

static GHashTable *type_handlers;

/** Load the named type of entries using the given function.
 * func is a void (*func)(gchar *role, xmlNodePtr *node)
 */
static void
persistence_load_type(xmlNodePtr node)
{
  gchar *typename = node->name;
  gchar *name;

  PersistenceLoadFunc func =
    (PersistenceLoadFunc)g_hash_table_lookup(type_handlers, typename);
  if (func == NULL) {
    return;
  }

  name = xmlGetProp(node, "role");
  if (name == NULL) {
    return;
  }
  
  (*func)(name, node);
  node = node->next;
}

static void
persistence_set_type_handler(gchar *name, PersistenceLoadFunc func)
{
  if (type_handlers == NULL)
    type_handlers = g_hash_table_new(g_str_hash, g_str_equal);

  g_hash_table_insert(type_handlers, name, (gpointer)func);
}

static void
persistence_init()
{
  persistence_set_type_handler("window", persistence_load_window);
  persistence_set_type_handler("entrystring", persistence_load_entrystring);
  persistence_set_type_handler("list", persistence_load_list);
  persistence_set_type_handler("integer", persistence_load_integer);
  persistence_set_type_handler("real", persistence_load_real);
  persistence_set_type_handler("boolean", persistence_load_boolean);
  persistence_set_type_handler("string", persistence_load_string);
  persistence_set_type_handler("color", persistence_load_color);

  if (persistent_windows == NULL) {
    persistent_windows = g_hash_table_new(g_str_hash, g_str_equal);
  }
  if (persistent_entrystrings == NULL) {
    persistent_entrystrings = g_hash_table_new(g_str_hash, g_str_equal);
  }
  if (persistent_lists == NULL) {
    persistent_lists = g_hash_table_new(g_str_hash, g_str_equal);
  }
  if (persistent_integers == NULL) {
    persistent_integers = g_hash_table_new(g_str_hash, g_str_equal);
  }
  if (persistent_reals == NULL) {
    persistent_reals = g_hash_table_new(g_str_hash, g_str_equal);
  }
  if (persistent_booleans == NULL) {
    persistent_booleans = g_hash_table_new(g_str_hash, g_str_equal);
  }
  if (persistent_strings == NULL) {
    persistent_strings = g_hash_table_new(g_str_hash, g_str_equal);
  }
  if (persistent_colors == NULL) {
    persistent_colors = g_hash_table_new(g_str_hash, g_str_equal);
  }
}

/* Load all persistent data. */
void
persistence_load()
{
  xmlDocPtr doc;
  gchar *filename = dia_config_filename("persistence");

  persistence_init();

  if (!g_file_test(filename, G_FILE_TEST_IS_REGULAR)) return;

  doc = xmlDiaParseFile(filename);
  if (doc != NULL) {
    if (doc->xmlRootNode != NULL) {
      xmlNsPtr namespace = xmlSearchNs(doc, doc->xmlRootNode, "dia");
      if (!strcmp (doc->xmlRootNode->name, "persistence") &&
	  namespace != NULL) {
	xmlNodePtr child_node = doc->xmlRootNode->children;
	for (; child_node != NULL; child_node = child_node->next) {
	  persistence_load_type(child_node);
	}
      }
    }
    xmlFreeDoc(doc);
  }
  g_free(filename);
}

/* *********************** SAVING FUNCTIONS *********************** */

/* Save the position of a window  */
static void
persistence_save_window(gpointer key, gpointer value, gpointer data)
{  
  xmlNodePtr tree = (xmlNodePtr)data;
  PersistentWindow *window_pos = (PersistentWindow *)value;
  ObjectNode window;

  window = (ObjectNode)xmlNewChild(tree, NULL, "window", NULL);
  
  xmlSetProp(window, "role", (char *)key);
  data_add_int(new_attribute(window, "xpos"), window_pos->x);
  data_add_int(new_attribute(window, "ypos"), window_pos->y);
  data_add_int(new_attribute(window, "width"), window_pos->width);
  data_add_int(new_attribute(window, "height"), window_pos->height);
  data_add_boolean(new_attribute(window, "isopen"), window_pos->isopen);

}

/* Save the contents of a string  */
static void
persistence_save_list(gpointer key, gpointer value, gpointer data)
{  
  xmlNodePtr tree = (xmlNodePtr)data;
  ObjectNode listnode;
  GString *buf;
  GList *items;

  listnode = (ObjectNode)xmlNewChild(tree, NULL, "list", NULL);

  xmlSetProp(listnode, "role", (char *)key);
  /* Make a string out of the list */
  buf = g_string_new("");
  for (items = ((PersistentList*)value)->glist; items != NULL;
       items = g_list_next(items)) {
    g_string_append(buf, (gchar *)items->data);
    if (g_list_next(items) != NULL) g_string_append(buf, "\n");
  }
  
  data_add_string(new_attribute(listnode, "listvalue"), buf->str);
  g_string_free(buf, TRUE);
}

static void
persistence_save_entrystring(gpointer key, gpointer value, gpointer data)
{  
  xmlNodePtr tree = (xmlNodePtr)data;
  ObjectNode stringnode;

  stringnode = (ObjectNode)xmlNewChild(tree, NULL, "entrystring", NULL);

  xmlSetProp(stringnode, "role", (char *)key);
  data_add_string(new_attribute(stringnode, "stringvalue"), (char *)value);
}

static void
persistence_save_integer(gpointer key, gpointer value, gpointer data)
{  
  xmlNodePtr tree = (xmlNodePtr)data;
  ObjectNode integernode;

  integernode = (ObjectNode)xmlNewChild(tree, NULL, "integer", NULL);

  xmlSetProp(integernode, "role", (char *)key);
  data_add_int(new_attribute(integernode, "intvalue"), *(gint *)value);
}

static void
persistence_save_real(gpointer key, gpointer value, gpointer data)
{  
  xmlNodePtr tree = (xmlNodePtr)data;
  ObjectNode realnode;

  realnode = (ObjectNode)xmlNewChild(tree, NULL, "real", NULL);

  xmlSetProp(realnode, "role", (char *)key);
  data_add_real(new_attribute(realnode, "realvalue"), *(real *)value);
}

static void
persistence_save_boolean(gpointer key, gpointer value, gpointer data)
{  
  xmlNodePtr tree = (xmlNodePtr)data;
  ObjectNode booleannode;

  booleannode = (ObjectNode)xmlNewChild(tree, NULL, "boolean", NULL);

  xmlSetProp(booleannode, "role", (char *)key);
  data_add_boolean(new_attribute(booleannode, "booleanvalue"), *(gboolean *)value);
}

static void
persistence_save_string(gpointer key, gpointer value, gpointer data)
{  
  xmlNodePtr tree = (xmlNodePtr)data;
  ObjectNode stringnode;

  stringnode = (ObjectNode)xmlNewChild(tree, NULL, "string", NULL);

  xmlSetProp(stringnode, "role", (char *)key);
  data_add_string(new_attribute(stringnode, "stringvalue"), (gchar *)value);
}

static void
persistence_save_color(gpointer key, gpointer value, gpointer data)
{  
  xmlNodePtr tree = (xmlNodePtr)data;
  ObjectNode colornode;

  colornode = (ObjectNode)xmlNewChild(tree, NULL, "color", NULL);

  xmlSetProp(colornode, "role", (char *)key);
  data_add_color(new_attribute(colornode, "colorvalue"), (Color *)value);
}


void
persistence_save_type(xmlDocPtr doc, GHashTable *entries, GHFunc func)
{
  if (entries != NULL && g_hash_table_size(entries) != 0) {
    g_hash_table_foreach(entries, func, doc->xmlRootNode);
  }
}

/* Save all persistent data. */
void
persistence_save()
{
  xmlDocPtr doc;
  xmlNs *name_space;
  gchar *filename = dia_config_filename("persistence");

  doc = xmlNewDoc("1.0");
  doc->encoding = xmlStrdup("UTF-8");
  doc->xmlRootNode = xmlNewDocNode(doc, NULL, "persistence", NULL);

  name_space = xmlNewNs(doc->xmlRootNode, 
                        "http://www.gnome.org/projects/dia",
			"dia");
  xmlSetNs(doc->xmlRootNode, name_space);

  persistence_save_type(doc, persistent_windows, persistence_save_window);
  persistence_save_type(doc, persistent_entrystrings, persistence_save_string);
  persistence_save_type(doc, persistent_lists, persistence_save_list);
  persistence_save_type(doc, persistent_integers, persistence_save_integer);
  persistence_save_type(doc, persistent_reals, persistence_save_real);
  persistence_save_type(doc, persistent_booleans, persistence_save_boolean);
  persistence_save_type(doc, persistent_strings, persistence_save_string);
  persistence_save_type(doc, persistent_colors, persistence_save_color);

  xmlDiaSaveFile(filename, doc);
  g_free(filename);
  xmlFreeDoc(doc);
}

/* *********************** USAGE FUNCTIONS *********************** */

/* ********* WINDOWS ********* */

/* Returns the name used for a window in persistence.
 */
static gchar *
persistence_get_window_name(GtkWindow *window)
{
  gchar *name = gtk_window_get_role(window);
  if (name == NULL) {
    printf("Internal:  Window %s has no role.\n", gtk_window_get_title(window));
    return NULL;
  }
  return name;
}

static void
persistence_store_window_info(GtkWindow *window, PersistentWindow *wininfo,
			      gboolean isclosed)
{
  /* Drawable means visible & mapped, what we usually think of as open. */
  if (!isclosed) {
    gtk_window_get_position(window, &wininfo->x, &wininfo->y);
    gtk_window_get_size(window, &wininfo->width, &wininfo->height);
    wininfo->isopen = TRUE;
  } else {
    wininfo->isopen = FALSE;
  }
}

static gboolean
persistence_update_window(GtkWindow *window, GdkEvent *event, gpointer data)
{
  gchar *name = persistence_get_window_name(window);
  PersistentWindow *wininfo;
  gboolean isclosed;

  if (name == NULL) return FALSE;
  if (persistent_windows == NULL) {
    persistent_windows = g_hash_table_new(g_str_hash, g_str_equal);
  }    
  wininfo = (PersistentWindow *)g_hash_table_lookup(persistent_windows, name);

  /* Can't tell the window state from the window itself yet. */
  isclosed = (event->type == GDK_UNMAP);
  if (wininfo != NULL) {
    persistence_store_window_info(window, wininfo, isclosed);
  } else {
    wininfo = g_new0(PersistentWindow, 1);
    persistence_store_window_info(window, wininfo, isclosed);
    g_hash_table_insert(persistent_windows, name, wininfo);
  }
  if (wininfo->window != NULL && wininfo->window != window) {
    g_object_unref(wininfo->window);
    wininfo->window = NULL;
  }
  if (wininfo->window == NULL) {
    wininfo->window = window;
    g_object_ref(window);
  }
  return FALSE;
}

/* Call this function after the window has a role assigned to use any
 * persistence information about the window.
 */
void
persistence_register_window(GtkWindow *window)
{
  gchar *name = persistence_get_window_name(window);
  PersistentWindow *wininfo;

  if (name == NULL) return;
  if (persistent_windows == NULL) {
    persistent_windows = g_hash_table_new(g_str_hash, g_str_equal);
  }    
  wininfo = (PersistentWindow *)g_hash_table_lookup(persistent_windows, name);
  if (wininfo != NULL) {
    gtk_window_move(window, wininfo->x, wininfo->y);
    gtk_window_resize(window, wininfo->width, wininfo->height);
    if (wininfo->isopen) gtk_widget_show(GTK_WIDGET(window));
  } else {
    wininfo = g_new0(PersistentWindow, 1);
    gtk_window_get_position(window, &wininfo->x, &wininfo->y);
    gtk_window_get_size(window, &wininfo->width, &wininfo->height);
    /* Drawable means visible & mapped, what we usually think of as open. */
    wininfo->isopen = GTK_WIDGET_DRAWABLE(GTK_WIDGET(window));
    g_hash_table_insert(persistent_windows, name, wininfo);
  }
  if (wininfo->window != NULL && wininfo->window != window) {
    g_object_unref(wininfo->window);
    wininfo->window = NULL;
  }
  if (wininfo->window == NULL) {
    wininfo->window = window;
    g_object_ref(window);
  }

  g_signal_connect(GTK_OBJECT(window), "configure-event",
		   G_CALLBACK(persistence_update_window), NULL);

  g_signal_connect(GTK_OBJECT(window), "unmap-event",
		   G_CALLBACK(persistence_update_window), NULL);
}

/** Call this function at start-up to have a window creation function
 * called if the window should be opened at startup.
 * If no persistence information is available for the given role,
 * nothing happens.
 * @arg role The role of the window, as will be set by gtk_window_set_role()
 * @arg createfunc A 0-argument function that creates the window.  This
 * function will be called if the persistence information indicates that the
 * window should be open.  The function should create and show the window.
 */
void
persistence_register_window_create(gchar *role, NullaryFunc *func)
{
  PersistentWindow *wininfo;

  if (role == NULL) return;
  if (persistent_windows == NULL) return;
  wininfo = (PersistentWindow *)g_hash_table_lookup(persistent_windows, role);
  if (wininfo != NULL) {
    (*func)();
  }
}


/* ********* STRING ENTRIES ********** */

static gboolean
persistence_update_string_entry(GtkWidget *widget, GdkEvent *event,
				gpointer userdata)
{
  gchar *role = (gchar*)userdata;

  if (event->type == GDK_FOCUS_CHANGE) {
    gchar *string = (gchar *)g_hash_table_lookup(persistent_entrystrings, role);
    gchar *entrystring = gtk_entry_get_text(GTK_ENTRY(widget));
    if (string == NULL || strcmp(string, entrystring)) {
      g_hash_table_insert(persistent_entrystrings, role, g_strdup(entrystring));
      if (string != NULL) g_free(string);
    }
  }

  return FALSE;
}

/** Change the contents of the persistently stored string entry.
 * If widget is non-null, it is updated to reflect the change.
 * This can be used e.g. for when a dialog is cancelled and the old
 * contents should be restored.
 */
gboolean
persistence_change_string_entry(gchar *role, gchar *string,
				GtkWidget *widget)
{
  gchar *old_string = (gchar*)g_hash_table_lookup(persistent_entrystrings, role);
  if (old_string != NULL) {
    if (widget != NULL) {
      gtk_entry_set_text(GTK_ENTRY(widget), string);
    }
    g_hash_table_insert(persistent_entrystrings, role, g_strdup(string));
    g_free(old_string);
  }

  return FALSE;
}

/** Register a string in a GtkEntry for persistence.
 * This should include not only a unique name, but some way to update
 * whereever the string is used.
 */
void
persistence_register_string_entry(gchar *role, GtkWidget *entry)
{
  gchar *string;
  if (role == NULL) return;
  if (persistent_entrystrings == NULL) {
    persistent_entrystrings = g_hash_table_new(g_str_hash, g_str_equal);
  }    
  string = (gchar *)g_hash_table_lookup(persistent_entrystrings, role);
  if (string != NULL) {
    gtk_entry_set_text(GTK_ENTRY(entry), string);
  } else {
    string = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry)));
    g_hash_table_insert(persistent_entrystrings, role, string);
  }
  g_signal_connect(G_OBJECT(entry), "event", 
		   G_CALLBACK(persistence_update_string_entry), role);
}

/* ********* LISTS ********** */

/* Lists are used for e.g. recent files, selected fonts, etc. 
 * Anywhere where the user occasionally picks from a long list and
 * is likely to reuse the items.
 */

PersistentList *
persistence_register_list(const gchar *role)
{
  PersistentList *list;
  if (role == NULL) return NULL;
  if (persistent_lists == NULL) {
    persistent_lists = g_hash_table_new(g_str_hash, g_str_equal);
  } else {   
    list = (PersistentList *)g_hash_table_lookup(persistent_lists, role);
    if (list != NULL) {
      return list;
    }
  }
  list = g_new(PersistentList, 1);
  list->role = role;
  list->glist = NULL;
  list->sorted = FALSE;
  list->max_members = G_MAXINT;
  g_hash_table_insert(persistent_lists, role, list);
  return list;
}

PersistentList *
persistent_list_get(const gchar *role)
{
  PersistentList *list;
  if (role == NULL) return NULL;
  if (persistent_lists != NULL) {
    list = (PersistentList *)g_hash_table_lookup(persistent_lists, role);
    if (list != NULL) {
      return list;
    }
  }
  /* Not registered! */
  return NULL;
}

GList *
persistent_list_get_glist(const gchar *role)
{
  PersistentList *plist = persistent_list_get(role);
  return plist->glist;
}

static GList *
persistent_list_cut_length(GList *list, gint length)
{
  while (g_list_length(list) > length) {
    GList *last = g_list_last(list);
    list = g_list_remove_link(list, last);
    g_list_free(last);
  }
  return list;
}

void
persistent_list_add(const gchar *role, const gchar *item)
{
  PersistentList *plist = persistent_list_get(role);
  if(plist == NULL) printf("Can't find list for %s when adding %s\n", 
			   role, item);
  if (plist->sorted) {
    /* Sorting not implemented yet. */
  } else {
    GList *tmplist = plist->glist;
    GList *old_elem = g_list_find_custom(tmplist, item, strcmp);
    while (old_elem != NULL) {
      tmplist = g_list_remove_link(tmplist, old_elem);
      /* Don't free this, as it makes recent_files go boom after
       * selecting a file there several times.  Yes, it should be strdup'd,
       * but it isn't.
       */
      /*g_free(old_elem->data);*/
      g_list_free_1(old_elem);
      old_elem = g_list_find_custom(tmplist, item, strcmp);
    }
    tmplist = g_list_prepend(tmplist, g_strdup(item));
    tmplist = persistent_list_cut_length(tmplist, plist->max_members);
    plist->glist = tmplist;
  }
}

void
persistent_list_set_max_length(const gchar *role, gint max)
{
  PersistentList *plist = persistent_list_get(role);
  plist->max_members = max;
  plist->glist = persistent_list_cut_length(plist->glist, max);
}

void
persistent_list_remove(const gchar *role, const gchar *item)
{
  PersistentList *plist = persistent_list_get(role);
  plist->glist = g_list_remove(plist->glist, item);
}

/* ********* INTEGERS ********** */
gint
persistence_register_integer(gchar *role, int defaultvalue)
{
  gint *integer;
  if (role == NULL) return 0;
  if (persistent_integers == NULL) {
    persistent_integers = g_hash_table_new(g_str_hash, g_str_equal);
  }    
  integer = (gint *)g_hash_table_lookup(persistent_integers, role);
  if (integer == NULL) {
    integer = g_new(gint, 1);
    *integer = defaultvalue;
    g_hash_table_insert(persistent_integers, role, integer);
  }
  return *integer;
}

gint
persistence_get_integer(gchar *role)
{
  gint *integer;
  if (persistent_integers == NULL) {
    printf("No persistent integers to get for %s!\n", role);
    return 0;
  }
  integer = (gint *)g_hash_table_lookup(persistent_integers, role);
  if (integer != NULL) return *integer;
  printf("No integer to get for %s\n", role);
  return 0;
}

void
persistence_set_integer(gchar *role, gint newvalue)
{
  gint *integer;
  if (persistent_integers == NULL) {
    printf("No persistent integers yet for %s!\n", role);
    return;
  }
  integer = (gint *)g_hash_table_lookup(persistent_integers, role);
  if (integer != NULL) *integer = newvalue;
  else printf("No integer to set for %s\n", role);
}

/* ********* REALS ********** */
real
persistence_register_real(gchar *role, real defaultvalue)
{
  real *realval;
  if (role == NULL) return 0;
  if (persistent_reals == NULL) {
    persistent_reals = g_hash_table_new(g_str_hash, g_str_equal);
  }    
  realval = (real *)g_hash_table_lookup(persistent_reals, role);
  if (realval == NULL) {
    realval = g_new(real, 1);
    *realval = defaultvalue;
    g_hash_table_insert(persistent_reals, role, realval);
  }
  return *realval;
}

real
persistence_get_real(gchar *role)
{
  real *realval;
  if (persistent_reals == NULL) {
    printf("No persistent reals to get for %s!\n", role);
    return 0;
  }
  realval = (real *)g_hash_table_lookup(persistent_reals, role);
  if (realval != NULL) return *realval;
  printf("No real to get for %s\n", role);
  return 0.0;
}

void
persistence_set_real(gchar *role, real newvalue)
{
  real *realval;
  if (persistent_reals == NULL) {
    printf("No persistent reals yet for %s!\n", role);
    return;
  }
  realval = (real *)g_hash_table_lookup(persistent_reals, role);
  if (realval != NULL) *realval = newvalue;
  else printf("No real to set for %s\n", role);
}


/* ********* BOOLEANS ********** */
gboolean
persistence_register_boolean(gchar *role, gboolean defaultvalue)
{
  gboolean *booleanval;
  if (role == NULL) return 0;
  if (persistent_booleans == NULL) {
    persistent_booleans = g_hash_table_new(g_str_hash, g_str_equal);
  }    
  booleanval = (gboolean *)g_hash_table_lookup(persistent_booleans, role);
  if (booleanval == NULL) {
    booleanval = g_new(gboolean, 1);
    *booleanval = defaultvalue;
    g_hash_table_insert(persistent_booleans, role, booleanval);
  }
  return *booleanval;
}

gboolean
persistence_get_boolean(gchar *role)
{
  gboolean *booleanval;
  if (persistent_booleans == NULL) {
    printf("No persistent booleans to get for %s!\n", role);
    return FALSE;
  }
  booleanval = (gboolean *)g_hash_table_lookup(persistent_booleans, role);
  if (booleanval != NULL) return *booleanval;
  printf("No boolean to get for %s\n", role);
  return FALSE;
}

void
persistence_set_boolean(gchar *role, gboolean newvalue)
{
  gboolean *booleanval;
  if (persistent_booleans == NULL) {
    printf("No persistent booleans yet for %s!\n", role);
    return;
  }
  booleanval = (gboolean *)g_hash_table_lookup(persistent_booleans, role);
  if (booleanval != NULL) *booleanval = newvalue;
  else printf("No boolean to set for %s\n", role);
}

/* ********* STRINGS ********** */
gchar *
persistence_register_string(gchar *role, gchar *defaultvalue)
{
  gchar *stringval;
  if (role == NULL) return 0;
  if (persistent_strings == NULL) {
    persistent_strings = g_hash_table_new(g_str_hash, g_str_equal);
  }    
  stringval = (gchar *)g_hash_table_lookup(persistent_strings, role);
  if (stringval == NULL) {
    stringval = g_strdup(defaultvalue);
    g_hash_table_insert(persistent_strings, role, stringval);
  }
  return stringval;
}

gchar *
persistence_get_string(gchar *role)
{
  gchar *stringval;
  if (persistent_strings == NULL) {
    printf("No persistent strings to get for %s!\n", role);
    return NULL;
  }
  stringval = (gchar *)g_hash_table_lookup(persistent_strings, role);
  if (stringval != NULL) return stringval;
  printf("No string to get for %s\n", role);
  return NULL;
}

void
persistence_set_string(gchar *role, gchar *newvalue)
{
  gchar *stringval;
  if (persistent_strings == NULL) {
    printf("No persistent strings yet for %s!\n", role);
    return;
  }
  stringval = (gchar *)g_hash_table_lookup(persistent_strings, role);
  if (stringval != NULL) {
    g_hash_table_insert(persistent_strings, role, g_strdup(newvalue));
    g_free(stringval);
  }
  else printf("No string to set for %s\n", role);
}

/* ********* COLORS ********** */
/* Remember that colors returned are private, not to be deallocated.
 * They will be smashed in some undefined way by persistence_set_color */
Color *
persistence_register_color(gchar *role, Color *defaultvalue)
{
  Color *colorval;
  if (role == NULL) return 0;
  if (persistent_colors == NULL) {
    persistent_colors = g_hash_table_new(g_str_hash, g_str_equal);
  }    
  colorval = (Color *)g_hash_table_lookup(persistent_colors, role);
  if (colorval == NULL) {
    colorval = g_new(Color, 1);
    *colorval = *defaultvalue;
    g_hash_table_insert(persistent_colors, role, colorval);
  }
  return colorval;
}

Color *
persistence_get_color(gchar *role)
{
  Color *colorval;
  if (persistent_colors == NULL) {
    printf("No persistent colors to get for %s!\n", role);
    return 0;
  }
  colorval = (Color *)g_hash_table_lookup(persistent_colors, role);
  if (colorval != NULL) return colorval;
  printf("No color to get for %s\n", role);
  return 0;
}

void
persistence_set_color(gchar *role, Color *newvalue)
{
  Color *colorval;
  if (persistent_colors == NULL) {
    printf("No persistent colors yet for %s!\n", role);
    return;
  }
  colorval = (Color *)g_hash_table_lookup(persistent_colors, role);
  if (colorval != NULL) *colorval = *newvalue;
  else printf("No color to set for %s\n", role);
}