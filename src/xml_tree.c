/*
 * File:    xml_tree.c
 * Author:  Li XianJing <xianjimli@hotmail.com>
 * Brief:   xml tree
 *
 * Copyright (c) Li XianJing
 *
 * Licensed under the Academic Free License version 2.1
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 * History:
 * ================================================================
 * 2009-06-22 Li XianJing <xianjimli@hotmail.com> created.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xml_tree.h"

XmlNode* xml_node_create_text(const char* text, size_t length)
{
	XmlNode* node = NULL;
	return_val_if_fail(text != NULL, NULL);

	if((node = calloc(1, sizeof(XmlNode))) != NULL)
	{
		node->type = XML_NODE_TEXT;
		node->u.text = strndup(text, length);
	}

	return node;
}

XmlNode* xml_node_create_comment(const char* comment, size_t length)
{
	XmlNode* node = NULL;
	return_val_if_fail(comment != NULL, NULL);

	if((node = calloc(1, sizeof(XmlNode))) != NULL)
	{
		node->type = XML_NODE_COMMENT;
		node->u.comment = strndup(comment, length);
	}

	return node;
}

void xml_node_append_attr(XmlNode* node, const char* name, const char* value)
{
	XmlAttr* attr = NULL;
	XmlAttr* iter = NULL;
	return_if_fail(node != NULL 
		&& (node->type == XML_NODE_PI || node->type == XML_NODE_NORMAL)
		&& name != NULL && value != NULL);
	
	attr = calloc(1, sizeof(XmlAttr));
	return_if_fail(attr != NULL);
	attr->name = strdup(name);
	attr->value = strdup(value);

	if(node->type == XML_NODE_PI)
	{
		if(node->u.pi.first_attr == NULL)
		{
			node->u.pi.first_attr = attr;
			return;
		}
		iter = node->u.pi.first_attr;
	}
	else
	{
		if(node->u.normal.first_attr == NULL)
		{
			node->u.normal.first_attr = attr;
			return;
		}
		iter = node->u.normal.first_attr;
	}

	while(iter->next != NULL) iter = iter->next;
	iter->next = attr;

	return;
}

XmlNode* xml_node_create_pi(const char* name, const char** attrs)
{
	XmlNode* node = NULL;
	return_val_if_fail(name != NULL, NULL);

	if((node = calloc(1, sizeof(XmlNode))) != NULL)
	{
		int i = 0;
		node->type = XML_NODE_PI;
		node->u.pi.name = strdup(name);
		if(attrs != NULL)
		{
			for(i = 0; attrs[i] != NULL && attrs[i+1] != NULL; i += 2)
			{
				xml_node_append_attr(node, attrs[i], attrs[i+1]);
			}
		}
	}

	return node;
}

XmlNode* xml_node_create_normal(const char* name, const char** attrs)
{
	XmlNode* node = NULL;
	return_val_if_fail(name != NULL, NULL);

	if((node = calloc(1, sizeof(XmlNode))) != NULL)
	{
		int i = 0;
		node->type = XML_NODE_NORMAL;
		node->u.normal.name = strdup(name);

		if(attrs != NULL)
		{
			for(i = 0; attrs[i] != NULL && attrs[i+1] != NULL; i += 2)
			{
				xml_node_append_attr(node, attrs[i], attrs[i+1]);
			}
		}
	}

	return node;
}

XmlNode* xml_node_append_sibling(XmlNode* node, XmlNode* sibling)
{
	return_val_if_fail(node != NULL && sibling != NULL, NULL);

	if(node->sibling == NULL)
	{
		node->sibling = sibling;
	}
	else
	{
		XmlNode* iter = node->sibling;
		while(iter->sibling != NULL) iter = iter->sibling;
		iter->sibling = sibling;
	}

	sibling->parent = node->parent;

	return sibling;
}

XmlNode* xml_node_append_child(XmlNode* node, XmlNode* child)
{
	return_val_if_fail(node != NULL && child != NULL, NULL);

	if(node->children == NULL)
	{
		node->children = child;
	}
	else
	{
		XmlNode* iter = node->children;
		while(iter->sibling != NULL) iter = iter->sibling;
		iter->sibling = child;
	}
	
	child->parent = node;

	return child;
}

void xml_node_visit(XmlNode* node, XmlBuilder* builder)
{
	return_if_fail(node != NULL && builder != NULL);
	
	switch(node->type)
	{
		case XML_NODE_NORMAL:
		{
			int i = 0;
			const char* attrs[2*MAX_ATTR_NR+1] = {0};
			XmlAttr* iter = node->u.normal.first_attr;
			for(; iter != NULL && i < MAX_ATTR_NR; iter = iter->next)
			{
				attrs[i++] = iter->name;
				attrs[i++] = iter->value;
			}

			xml_builder_on_start_element(builder, node->u.normal.name, attrs);
			break;
		}
		case XML_NODE_PI:
		{
			int i = 0;
			const char* attrs[2*MAX_ATTR_NR+1] = {0};
			XmlAttr* iter = node->u.pi.first_attr;
			for(; iter != NULL && i < MAX_ATTR_NR; iter = iter->next)
			{
				attrs[i++] = iter->name;
				attrs[i++] = iter->value;
			}
			xml_builder_on_pi_element(builder, node->u.pi.name, attrs);
			break;
		}
		case XML_NODE_TEXT:
		{
			xml_builder_on_text(builder, node->u.text, strlen(node->u.text));
			break;
		}
		case XML_NODE_COMMENT:
		{
			xml_builder_on_comment(builder, node->u.comment, strlen(node->u.comment));
			break;
		}
		default:break;
	}

	if(node->children != NULL)
	{
		xml_node_visit(node->children, builder);
	}

	if(node->type == XML_NODE_NORMAL)
	{
		xml_builder_on_end_element(builder, node->u.normal.name);
	}

	if(node->sibling != NULL)
	{
		xml_node_visit(node->sibling, builder);
	}

	return;
}

void xml_attr_destroy(XmlAttr* attr)
{
	XmlAttr* next = attr;
	
	while(attr != NULL)
	{
		next = attr->next;
		free(attr->name);
		free(attr->value);
		free(attr);
		attr = next;
	}

	return;
}

void xml_node_destroy(XmlNode* node)
{
	if(node == NULL)
	{
		return;
	}

	if(node->children != NULL)
	{
		xml_node_destroy(node->children);
		node->children = NULL;
	}

	if(node->sibling != NULL)
	{
		xml_node_destroy(node->sibling);
		node->sibling = NULL;
	}

	switch(node->type)
	{
		case XML_NODE_TEXT:
		{
			free(node->u.text);
			break;
		}
		case XML_NODE_COMMENT:
		{
			free(node->u.comment);
			break;
		}
		case XML_NODE_PI:
		{
			free(node->u.pi.name);
			xml_attr_destroy(node->u.pi.first_attr);
			break;
		}
		case XML_NODE_NORMAL:
		{
			free(node->u.normal.name);
			xml_attr_destroy(node->u.normal.first_attr);
			break;
		}
		default:break;
	}

	free(node);

	return;
}

//wupeng add
XmlNode* xml_tree_find(XmlNode* node, const char* name, int index)
{
	return_val_if_fail(node!=NULL && name!=NULL && index>=0, NULL);

	XmlNode* cur_node = node->children;	
	int i = -1;
	while(cur_node != NULL)
	{
		if(cur_node->type == XML_NODE_NORMAL 
				&& strcmp(cur_node->u.normal.name, name) == 0)
		{
			i++;
		}
		if(i == index) 
		{
			return cur_node;
		}

		cur_node = cur_node->sibling;
	}

	return NULL;
}

XmlNode* xml_tree_find_first(XmlNode* node, const char* name)
{
	return xml_tree_find(node, name, 0);
}

char* xml_tree_str(XmlNode* node, const char* name, int index)
{
	return_val_if_fail(node!=NULL && name!=NULL && index>=0, NULL);

	XmlNode* cur_node = xml_tree_find(node, name, index);
	if(cur_node == NULL) return NULL;


	cur_node = cur_node->children;
	if(cur_node!=NULL && cur_node->type==XML_NODE_TEXT)
		return cur_node->u.text;
	else
		return NULL;
}

char* xml_tree_str_first(XmlNode* node, const char* name)
{
	return xml_tree_str(node, name, 0);
}

int xml_tree_int(XmlNode* node, const char* name, int index, int default_val)
{
	char* str = xml_tree_str(node, name, index);

	if(str == NULL) return default_val;

	return atoi(str);
}

int xml_tree_int_first(XmlNode* node, const char* name, int default_val)
{
	return xml_tree_int(node, name, 0, default_val);
}

#ifdef XML_TREE_TEST
#include <assert.h>
#include "xml_builder_dump.h"

int main(int argc, char* argv[])
{
	const char* attrs[] = {"name", "xxxx", "age", "100", NULL};
	const char* pi_attrs[] = {"version", "1.0", "encoding", "utf-8", NULL};

	XmlNode* pi = xml_node_create_pi("xml", pi_attrs);
	XmlNode* root = xml_node_create_normal("root", NULL);

	assert(pi != NULL);
	assert(pi->type == XML_NODE_PI);
	assert(strcmp(pi->u.normal.name, "xml") == 0);

	xml_node_append_sibling(pi, root);
	assert(root != NULL);
	assert(root->type == XML_NODE_NORMAL);
	assert(strcmp(root->u.normal.name, "root") == 0);

	XmlNode* child1 = xml_node_create_normal("child1", attrs);
	XmlNode* child2 = xml_node_create_normal("child2", attrs);
	xml_node_append_child(root, child1);
	assert(root->children == child1);
	xml_node_append_sibling(child1, child2);
	assert(child1->sibling == child2);

	XmlBuilder* dump_builder = xml_builder_dump_create(stdout);
	xml_node_visit(pi, dump_builder);
	xml_node_destroy(pi);

	return 0;
}
#endif/*XML_TREE_TEST*/

