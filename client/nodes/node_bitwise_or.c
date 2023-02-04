/**
 * @file	examples/nodes/node_bitwise_or.c
 * @author  Armin Luntzer (armin.luntzer@univie.ac.at)
 *
 * @copyright GPLv2
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * @brief a GtkNode which outputs at C the result of a bitwise OR to
 *	  the values at gates A and B. Note that the considered data width
 *	  is restricted to the first 8 bits of the input data
 */

#include <gtk/gtk.h>
#include <gtknode.h>
#include <gtknodesocket.h>

#include <nodes_common.h>
#include <node_bitwise_or.h>

typedef struct _NodeBitwiseOrInput	NodeBitwiseOrInput;

struct _NodeBitwiseOrPrivate {

	GtkWidget  *GateA;
	GtkWidget  *GateB;
	GtkWidget  *GateC;

	gint8 A;
	gint8 B;
	gint8 *C;

	GByteArray *payload;

	GList *inputs;
};



G_DEFINE_TYPE_WITH_PRIVATE(NodeBitwiseOr, node_bitwise_or, GTKNODES_TYPE_NODE)



static void node_bitwise_or_output(NodeBitwiseOr *bitwise_or)
{
	NodeBitwiseOrPrivate *priv;


	priv = bitwise_or->priv;

	gtk_nodes_node_socket_write(GTKNODES_NODE_SOCKET(priv->GateC),
					priv->payload);
}


static void node_bitwise_or_output_connected(GtkWidget	  *socket,
						  GtkWidget	  *source,
						  NodeBitwiseOr *bitwise_or)
{
	node_bitwise_or_output(bitwise_or);
}


static void node_bitwise_or_input(GtkWidget	  *widget,
				   GByteArray	 *payload,
				   NodeBitwiseOr *bitwise_or)
{
	guint8 ibyte;
	NodeBitwiseOrPrivate *priv;


	if (!payload)
		return;

	if (!payload->len)
		return;


	priv = bitwise_or->priv;

	ibyte = ((guint8 *) payload->data)[0];

	if (widget == priv->GateA)
		priv->A = ibyte;

	if (widget == priv->GateB)
		priv->B = ibyte;

	(*priv->C) = priv->A | priv->B;

	if (ibyte)
		gtk_nodes_node_socket_set_rgba(GTKNODES_NODE_SOCKET(widget),
						   &node_red);
	else
		gtk_nodes_node_socket_set_rgba(GTKNODES_NODE_SOCKET(widget),
						   &node_yellow);

	if ((*priv->C))
		gtk_nodes_node_socket_set_rgba(GTKNODES_NODE_SOCKET(priv->GateC),
						   &node_green);
	else
		gtk_nodes_node_socket_set_rgba(GTKNODES_NODE_SOCKET(priv->GateC),
						   &node_blue);


	node_bitwise_or_output(bitwise_or);
}


static void node_bitwise_or_remove(GtkWidget *bitwise_or,
					gpointer user_data)
{
	NodeBitwiseOrPrivate *priv;


	priv = NODE_LOGICAL_OR(bitwise_or)->priv;

	gtk_widget_destroy(bitwise_or);

	if (priv->payload) {
		g_byte_array_free (priv->payload, TRUE);
		priv->payload = NULL;
	}
}


static void node_bitwise_or_class_init(NodeBitwiseOrClass *klass)
{
	__attribute__((unused))
		GtkWidgetClass *widget_class;


	widget_class = GTK_WIDGET_CLASS(klass);

	/* override widget methods go here if needed */
}


static void node_bitwise_or_init(NodeBitwiseOr *bitwise_or)
{
	GtkWidget *w;
	NodeBitwiseOrPrivate *priv;


	priv = bitwise_or->priv = node_bitwise_or_get_instance_private (bitwise_or);

	/* our output payload is constant allocate it here */
	priv->C = g_malloc0(sizeof(gint8));
	priv->payload = g_byte_array_new_take((void *) priv->C, sizeof(gint8));


	g_signal_connect(G_OBJECT(bitwise_or), "node-func-clicked",
			 G_CALLBACK(node_bitwise_or_remove), NULL);

	gtk_nodes_node_set_label(GTKNODES_NODE (bitwise_or),
				 "Bitwise OR");



	/* input gate A */
	w = gtk_label_new("Gate A");
	gtk_label_set_xalign(GTK_LABEL(w), 0.0);
	priv->GateA = gtk_nodes_node_item_add(GTKNODES_NODE(bitwise_or), w,
						  GTKNODES_NODE_SOCKET_SINK);
	gtk_nodes_node_socket_set_rgba(GTKNODES_NODE_SOCKET(priv->GateA),
					   &node_yellow);
	g_signal_connect(G_OBJECT(priv->GateA), "socket-incoming",
			 G_CALLBACK(node_bitwise_or_input), bitwise_or);



	/* output gate C */
	w = gtk_label_new("Gate C");
	gtk_label_set_xalign(GTK_LABEL(w), 1.0);
	priv->GateC = gtk_nodes_node_item_add(GTKNODES_NODE(bitwise_or), w,
						  GTKNODES_NODE_SOCKET_SOURCE);

	g_signal_connect(G_OBJECT(priv->GateC), "socket-connect",
			 G_CALLBACK(node_bitwise_or_output_connected),
			 bitwise_or);

	gtk_nodes_node_socket_set_rgba(GTKNODES_NODE_SOCKET(priv->GateC),
					   &node_blue);


	/* input gate B */
	w = gtk_label_new("Gate B");
	gtk_label_set_xalign(GTK_LABEL(w), 0.0);
	priv->GateB = gtk_nodes_node_item_add(GTKNODES_NODE(bitwise_or), w,
						  GTKNODES_NODE_SOCKET_SINK);
	gtk_nodes_node_socket_set_rgba(GTKNODES_NODE_SOCKET(priv->GateB),
					   &node_yellow);
	gtk_box_set_child_packing(GTK_BOX(bitwise_or), w, FALSE, FALSE, 0,
				  GTK_PACK_END);
	g_signal_connect(G_OBJECT(priv->GateB), "socket-incoming",
			 G_CALLBACK(node_bitwise_or_input), bitwise_or);
}


GtkWidget *node_bitwise_or_new(void)
{
	NodeBitwiseOr *bitwise_or;


	bitwise_or = g_object_new(TYPE_NODE_LOGICAL_OR, NULL);

	return GTK_WIDGET(bitwise_or);
}
