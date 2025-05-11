// leaked list (c)
#ifndef _LIST_H
#define _LIST_H

#include "util.h"

struct list {
	struct node *head;
};

struct node {
	void *data;
	struct node *next;
};


struct list *prepend(struct list *l, void *val);
struct node *append(struct list *l, void *val);
void list_remove(struct list *l, struct node *item);
void print_list(struct list *l);
int list_length(struct list *l);
void list_free(struct list *l);


#ifdef LIST_IMPL
struct list *prepend(struct list *l, void *val)
{
	struct node *new = xmalloc(sizeof(struct node));
	new->data = val;
	new->next = l->head; //new node points to the old head
	
	l->head = new; //becomes the new head

	return l;
}


struct node *append(struct list *l, void *val)
{
	struct node *new = xmalloc(sizeof(struct node));
	new->data = val;
	new->next = NULL;

	struct node **p = &l->head;

	while(*p != NULL)
		p = &(*p)->next;
	*p = new;

	return *p;
}

void list_remove(struct list *l, struct node *item)
{
	struct node **p = &l->head;

	while (*p != item)
			p = &(*p)->next;
	*p = item->next;
}
/*
void print_list(struct list *l)
{
	struct node **p = &l->head; //address of the pointer to the first element
	// while the pointer to the next element in the list is not NULL (meaning the end of the list)
	while(*p != NULL) { 
		printf("%f\n", (*p)->data);
		p = &(*p)->next; //make p the address of the pointer to the next element
	}
}
*/

int list_length(struct list *l)
{
	int c = 0;
	struct node **p = &l->head;

	while (*p != NULL){
		c++;
		p = &(*p)->next;
	}
	return c;
}

void list_free(struct list *l)
{
	struct node *p = l->head;
	struct node *n;
	while (p != NULL)
	{
		n = p;
		p = p->next;
		free(n->data);
		free(n);
	}
	free(l);
}


#endif
#endif
