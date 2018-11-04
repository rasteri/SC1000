/*
 * Copyright (C) 2018 Mark Hills <mark@xwax.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 *
 * You should have received a copy of the GNU General Public License
 * version 2 along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 */

/*
 * Double-linked lists
 */

#ifndef LIST_H
#define LIST_H

#include <stdbool.h>
#include <stddef.h> /* offsetof() */

#define container_of(ptr, type, member) \
    ((type*)((char*)ptr - offsetof(type, member)))

struct list {
    struct list *prev, *next;
};

#define LIST_INIT(list) { \
    .next = &list, \
    .prev = &list \
}

static inline void list_init(struct list *list)
{
    list->next = list;
    list->prev = list;
}

static inline void __list_add(struct list *new, struct list *prev,
                              struct list *next)
{
    next->prev = new;
    new->next = next;
    new->prev = prev;
    prev->next = new;
}

/*
 * Insert a new list entry after the given head
 */

static inline void list_add(struct list *new, struct list *head)
{
    __list_add(new, head, head->next);
}

/*
 * Insert a new list entry before the given head (ie. end of list)
 */

static inline void list_add_tail(struct list *new, struct list *head)
{
    __list_add(new, head->prev, head);
}

static inline void list_del(struct list *entry)
{
    struct list *next, *prev;

    next = entry->next;
    prev = entry->prev;

    next->prev = prev;
    prev->next = next;
}

static inline bool list_empty(const struct list *head)
{
	return head->next == head;
}

#define list_entry(ptr, type, member) \
    container_of(ptr, type, member)

/*
 * Iterate through each item in the list
 */

#define list_for_each(item, head, member) \
    for (item = list_entry((head)->next, typeof(*item), member); \
         &item->member != (head); \
         item = list_entry(item->member.next, typeof(*item), member))

/*
 * Iterate through each item in the list, with a buffer to support
 * the safe removal of a list entry.
 *
 * pos: current entry (struct type*)
 * tmp: temporary storage (struct type*)
 * head: top of list (struct list*)
 * member: the name of the 'struct list' within 'struct type'
 */

#define list_for_each_safe(pos, tmp, head, member) \
    for (pos = list_entry((head)->next, typeof(*pos), member), \
         tmp = list_entry(pos->member.next, typeof(*pos), member); \
         &pos->member != (head); \
         pos = tmp, tmp = list_entry(tmp->member.next, typeof(*tmp), member))

#endif
