/* Dia -- an diagram creation/manipulation program
 * Copyright (C) 1998 Alexander Larsson
 *
 * umlattribute.c : refactored from uml.c, class.c to final use StdProps
 *                  PROP_TYPE_DARRAY, a list where each element is a set
 *                  of properies described by the same StdPropDesc
 * Copyright (C) 2005 Hans Breuer
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "uml.h"
#include "properties.h"

static PropDescription umlattribute_props[] = {
  { "name", PROP_TYPE_STRING, PROP_FLAG_VISIBLE | PROP_FLAG_OPTIONAL,
  N_("Name"), NULL, NULL },
  { "type", PROP_TYPE_STRING, PROP_FLAG_VISIBLE | PROP_FLAG_OPTIONAL,
  N_("Type"), NULL, NULL },
  { "value", PROP_TYPE_STRING, PROP_FLAG_VISIBLE | PROP_FLAG_OPTIONAL,
  N_("Value"), NULL, NULL },
  { "comment", PROP_TYPE_STRING, PROP_FLAG_VISIBLE | PROP_FLAG_OPTIONAL,
  N_("Comment"), NULL, NULL },
  { "visibility", PROP_TYPE_ENUM, PROP_FLAG_VISIBLE | PROP_FLAG_OPTIONAL,
  N_("Visibility"), NULL, NULL },
  { "abstract", PROP_TYPE_BOOL, PROP_FLAG_VISIBLE | PROP_FLAG_OPTIONAL,
  N_("Abstract (?)"), NULL, NULL },
  { "class_scope", PROP_TYPE_BOOL, PROP_FLAG_VISIBLE | PROP_FLAG_OPTIONAL,
  N_("Class scope (static)"), NULL, NULL },

  PROP_DESC_END
};

static PropOffset umlattribute_offsets[] = {
  { "name", PROP_TYPE_STRING, offsetof(UMLAttribute, name) },
  { "type", PROP_TYPE_STRING, offsetof(UMLAttribute, type) },
  { "value", PROP_TYPE_STRING, offsetof(UMLAttribute, value) },
  { "comment", PROP_TYPE_STRING, offsetof(UMLAttribute, comment) },
  { "visibility", PROP_TYPE_ENUM, offsetof(UMLAttribute, visibility) },
  { "abstract", PROP_TYPE_BOOL, offsetof(UMLAttribute, abstract) },
  { "class_scope", PROP_TYPE_BOOL, offsetof(UMLAttribute, class_scope) },
  { NULL, 0, 0 },
};


PropDescDArrayExtra umlattribute_extra = {
  umlattribute_props,
  umlattribute_offsets,
  "umlattribute",
  uml_attribute_new,
  uml_attribute_destroy
};


UMLAttribute *
uml_attribute_new(void)
{
  UMLAttribute *attr;
  
  attr = g_new0(UMLAttribute, 1);
  attr->name = g_strdup("");
  attr->type = g_strdup("");
  attr->value = NULL;
  attr->comment = g_strdup("");
  attr->visibility = UML_PUBLIC;
  attr->abstract = FALSE;
  attr->class_scope = FALSE;
  
  attr->left_connection = g_new0(ConnectionPoint,1);
  attr->right_connection = g_new0(ConnectionPoint,1);
  return attr;
}

UMLAttribute *
uml_attribute_copy(UMLAttribute *attr)
{
  UMLAttribute *newattr;

  newattr = g_new0(UMLAttribute, 1);
  newattr->name = g_strdup(attr->name);
  newattr->type = g_strdup(attr->type);
  if (attr->value != NULL) {
    newattr->value = g_strdup(attr->value);
  } else {
    newattr->value = NULL;
  }
  if (attr->comment != NULL)
    newattr->comment = g_strdup (attr->comment);
  else 
    newattr->comment = NULL;

  newattr->visibility = attr->visibility;
  newattr->abstract = attr->abstract;
  newattr->class_scope = attr->class_scope;

  newattr->left_connection = g_new0(ConnectionPoint,1);
  *newattr->left_connection = *attr->left_connection;
  newattr->left_connection->object = NULL; /* must be setup later */

  newattr->right_connection = g_new0(ConnectionPoint,1);
  *newattr->right_connection = *attr->right_connection;
  newattr->right_connection->object = NULL; /* must be setup later */
  
  return newattr;
}

void
uml_attribute_destroy(UMLAttribute *attr)
{
  g_free(attr->name);
  g_free(attr->type);
  if (attr->value != NULL)
    g_free(attr->value);
  if (attr->comment != NULL)
    g_free(attr->comment);
  g_free(attr->left_connection);
  g_free(attr->right_connection);
  g_free(attr);
}

void
uml_attribute_write(AttributeNode attr_node, UMLAttribute *attr)
{
  DataNode composite;

  composite = data_add_composite(attr_node, "umlattribute");

  data_add_string(composite_add_attribute(composite, "name"),
		  attr->name);
  data_add_string(composite_add_attribute(composite, "type"),
		  attr->type);
  data_add_string(composite_add_attribute(composite, "value"),
		  attr->value);
  data_add_string(composite_add_attribute(composite, "comment"),
		  attr->comment);
  data_add_enum(composite_add_attribute(composite, "visibility"),
		attr->visibility);
  data_add_boolean(composite_add_attribute(composite, "abstract"),
		  attr->abstract);
  data_add_boolean(composite_add_attribute(composite, "class_scope"),
		  attr->class_scope);
}

/* Warning, the following *must* be strictly ASCII characters (or fix the 
   following code for UTF-8 cleanliness */

char visible_char[] = { '+', '-', '#', ' ' };

char *
uml_get_attribute_string (UMLAttribute *attribute)
{
  int len;
  char *str;

  len = 1 + strlen (attribute->name) + strlen (attribute->type);
  if (attribute->name[0] && attribute->type[0]) {
    len += 2;
  }
  if (attribute->value != NULL && attribute->value[0] != '\0') {
    len += 3 + strlen (attribute->value);
  }
  
  str = g_malloc (sizeof (char) * (len + 1));

  str[0] = visible_char[(int) attribute->visibility];
  str[1] = 0;

  strcat (str, attribute->name);
  if (attribute->name[0] && attribute->type[0]) {
    strcat (str, ": ");
  }
  strcat (str, attribute->type);
  if (attribute->value != NULL && attribute->value[0] != '\0') {
    strcat (str, " = ");
    strcat (str, attribute->value);
  }
    
  g_assert (strlen (str) == len);

  return str;
}
