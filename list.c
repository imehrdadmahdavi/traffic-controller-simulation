#include "list.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

/* ----------------- Debug and Error Checking Macros ----------------- */
#ifdef NDEBUG
#define debug(M, ...)
#else
#define debug(M, ...) fprintf(stderr, "DEBUG %s:%d: " M "\n",\
        __FILE__, __LINE__, ##__VA_ARGS__)
#endif

#define clean_errno() (errno == 0 ? "None" : strerror(errno))

#define log_err(M, ...) fprintf(stderr,\
        "[ERROR] (%s:%d: errno: %s) " M "\n", __FILE__, __LINE__,\
        clean_errno(), ##__VA_ARGS__)

#define log_warn(M, ...) fprintf(stderr,\
        "[WARN] (%s:%d: errno: %s) " M "\n",\
        __FILE__, __LINE__, clean_errno(), ##__VA_ARGS__)

#define log_info(M, ...) fprintf(stderr, "[INFO] (%s:%d) " M "\n",\
        __FILE__, __LINE__, ##__VA_ARGS__)

#define check(A, M, ...) if(!(A)) {\
    log_err(M, ##__VA_ARGS__); errno=0; goto error; }

#define sentinel(M, ...)  { log_err(M, ##__VA_ARGS__);\
    errno=0; goto error; }

#define check_mem(A) check((A), "Out of memory.")

#define check_debug(A, M, ...) if(!(A)) { debug(M, ##__VA_ARGS__);\
    errno=0; goto error; }
/* ------------------------------------------------------------------- */


//Initialize the list into an empty list. Returns TRUE if all is well and
//returns FALSE if there is an error initializing the list.
int ListInit(List* list)
{
	ListElem* node = malloc(sizeof(ListElem));
	check_mem(node);
	node->obj = NULL;
	node->next = NULL;
	node->prev = NULL;

	list->anchor = *node;
	return 1;
error:
	return 0;
}

//If list is empty, just add obj to the list. Otherwise, add obj after Last().
//This function returns TRUE if the operation is performed successfully and
//returns FALSE otherwise.
int ListAppend(List* list, void* value)
{
	ListElem *node = malloc(sizeof(ListElem));
	check_mem(node);
	node->obj = value;

	if (ListEmpty(list)) {
		node->next = &(list->anchor);
		node->prev = &(list->anchor);
		list->anchor.next = node;
		list->anchor.prev = node;
	} else {
		ListElem* ptr_to_last_elem = ListLast(list);
		ptr_to_last_elem->next= node;

		node->next = &(list->anchor);
		node->prev = ptr_to_last_elem;

		list->anchor.prev = node;
	}
	list->num_members++;
	return 1;
error:
	return 0;
}

int  ListPrepend(List* list, void* value)
{
	ListElem *node = malloc(sizeof(ListElem));
	check_mem(node);
	node->obj = value;

	if (ListEmpty(list)) {
		node->next = &(list->anchor);
		node->prev = &(list->anchor);
		list->anchor.next = node;
		list->anchor.prev = node;

	} else {
		ListElem* ptr_to_first_elem = ListFirst(list);
		ptr_to_first_elem->prev = node;

		node->next = ptr_to_first_elem;
		node->prev = &(list->anchor);

		list->anchor.next = node;
	}

	list->num_members++;
	return 1;
error:
	return 0;
}

//Unlink and delete elem from the list. It doesn't delete the object pointed to
//by elem and do not check if elem is on the list.
void ListUnlink(List* list, ListElem* node)
{
	check(node || node != &(list->anchor), "node is NULL is Unlink!");
	node->prev->next = node->next;
	node->next->prev = node->prev;
	node->next = NULL;
	node->prev = NULL;

	list->num_members--;
error:
	if(!node) printf("%s\n", "Error in Unlink!");
}

//Unlink and delete all elements from the list and make the list empty. It
//doesn't delete the objects pointed to by the list elements.
void ListUnlinkAll(List* list)
{
	ListElem* pts_to_current = NULL;
	check(!ListEmpty(list), "List is empty in UnlinkAll!");
	pts_to_current = &(list->anchor);
	do {
	 ListUnlink(list, pts_to_current->next);
	} while (!ListEmpty(list));

error:
	if(!pts_to_current && !ListEmpty(list)) printf(
		"%s\n", "Error in UnlinkAll!");
}

//Insert obj between elem and elem->prev. If elem is NULL, then this is the same
//as Prepend(). This function returns TRUE if the operation is performed
//successfully and FALSE otherwise. It does not check if elem is on the list.
int ListInsertBefore(List* list, void* obj, ListElem* elem)
{
	//check(obj,"Error!");
	if(!elem) {
		ListPrepend(list, obj);
	} else {
		ListElem *node = malloc(sizeof(ListElem));
		check_mem(node);
		node->obj = obj;

		node->next = elem;
		node->prev = elem->prev;

		elem->prev->next = node;
		elem->prev = node;

		list->num_members++;
	}

	return 1;
error:
	return 0;
}

//Insert obj between elem and elem->next. If elem is NULL, then this is the same
//as Append(). This function returns TRUE if the operation is performed
//successfully and FALSE otherwise, It doesn't check if elem is on the list.
int ListInsertAfter(List* list, void* obj, ListElem* elem)
{
	// check(obj,"Error!");
	if(!elem) {
		ListAppend(list, obj);
	} else {
		ListElem *node = malloc(sizeof(ListElem));
		check_mem(node);
		node->obj = obj;

		node->next = elem->next;
		node->prev = elem;

		elem->next->prev = node;
		elem->next = node;

		list->num_members++;
	}
	return 1;
error:
	return 0;
}

int ListLength(List *list)
{
  return list->num_members;
}

int ListEmpty(List *list)
{
   return list->num_members <= 0;
}

//Returns the first list element or NULL if the list is empty
ListElem *ListFirst(List* list)
{
		if(ListEmpty(list)) return NULL;
		else return list->anchor.next;
}

//Returns the last list element or NULL if the list is empty
ListElem *ListLast(List* list)
{
	if(ListEmpty(list)) return NULL;
	else return list->anchor.prev;
}

//Returns elem->next or NULL if elem is the last item on the list. It doesn't
//check if elem is on the list
ListElem *ListNext(List* list, ListElem* elem)
{
	 if(elem == ListLast(list)) return NULL;
	 else return elem->next;
}

//Returns elem->prev or NULL if elem is the first item on the list. It doesn't
//check if elem is on the list
ListElem *ListPrev(List* list, ListElem* elem)
{
	if(elem == ListFirst(list)) return NULL;
	else return elem->prev;
}

//Returns the list element elem such that elem->obj == obj. Returns NULL if no
//such element can be found.
ListElem *ListFind(List* list, void* obj)
{
	ListElem* elem = ListFirst(list);

	while (elem != ListLast(list)->next) {
		if(elem->obj == obj) return elem;
		elem = elem->next;
	}
	return NULL;
}
