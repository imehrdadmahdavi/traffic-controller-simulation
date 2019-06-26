#ifndef _LIST_H_
#define _LIST_H_

#include "header.h"

typedef struct tagListElem {
    void *obj;
    struct tagListElem *next;
    struct tagListElem *prev;
} ListElem;

typedef struct tagList {
    int num_members;
    ListElem anchor;

    /* You do not have to set these function pointers */
    int  (*Length)(struct tagList *);
    int  (*Empty)(struct tagList *);

    int  (*Append)(struct tagList *, void*);
    int  (*Prepend)(struct tagList *, void*);
    void (*Unlink)(struct tagList *, ListElem*);
    void (*UnlinkAll)(struct tagList *);
    int  (*InsertBefore)(struct tagList *, void*, ListElem*);
    int  (*InsertAfter)(struct tagList *, void*, ListElem*);

    ListElem *(*First)(struct tagList *);
    ListElem *(*Last)(struct tagList *);
    ListElem *(*Next)(struct tagList *, ListElem *cur);
    ListElem *(*Prev)(struct tagList *, ListElem *cur);

    ListElem *(*Find)(struct tagList *, void *obj);
} List;

extern int  ListLength(List*);
extern int  ListEmpty(List*);

extern int  ListAppend(List*, void*);
extern int  ListPrepend(List*, void*);
extern void ListUnlink(List*, ListElem*);
extern void ListUnlinkAll(List*);
extern int  ListInsertAfter(List*, void*, ListElem*);
extern int  ListInsertBefore(List*, void*, ListElem*);

extern ListElem *ListFirst(List*);
extern ListElem *ListLast(List*);
extern ListElem *ListNext(List*, ListElem*);
extern ListElem *ListPrev(List*, ListElem*);

extern ListElem *ListFind(List*, void*);

extern int ListInit(List*);

#endif /*_LIST_H_*/
