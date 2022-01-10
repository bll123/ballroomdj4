#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "queue.h"

static void * queueRemove (queue_t *q, queuenode_t *node);


queue_t *
queueAlloc (queueFree_t freeHook)
{
  queue_t     *q;

  q = malloc (sizeof (queue_t));
  assert (q != NULL);
  q->count = 0;
  q->currentIndex = 0;
  q->iteratorNode = NULL;
  q->currentNode = NULL;
  q->head = NULL;
  q->tail = NULL;
  q->freeHook = freeHook;
  return q;
}

void
queueFree (queue_t *q)
{
  queuenode_t     *node;
  queuenode_t     *tnode;

  if (q != NULL) {
    node = q->head;
    tnode = node;
    while (node != NULL && node->next != NULL) {
      node = node->next;
      if (tnode != NULL) {
        if (tnode->data != NULL && q->freeHook != NULL) {
          q->freeHook (tnode->data);
        }
        free (tnode);
      }
      tnode = node;
    }
    if (tnode != NULL) {
      if (tnode->data != NULL && q->freeHook != NULL) {
        q->freeHook (tnode->data);
      }
      free (tnode);
    }
    free (q);
  }
}

void
queuePush (queue_t *q, void *data)
{
  queuenode_t     *node;

  if (q == NULL) {
    return;
  }

  node = malloc (sizeof (queuenode_t));
  assert (node != NULL);
  node->prev = q->tail;
  node->next = NULL;
  node->data = data;

  if (q->head == NULL) {
    q->head = node;
  }
  if (node->prev != NULL) {
    node->prev->next = node;
  }
  q->tail = node;
  q->count++;
}

void *
queueGetCurrent (queue_t *q)
{
  void          *data = NULL;
  queuenode_t   *node;

  if (q == NULL) {
    return NULL;
  }
  node = q->head;

  if (node != NULL) {
    data = node->data;
  }
  return data;
}

void *
queueGetByIdx (queue_t *q, long idx)
{
  long              count = 0;
  queuenode_t       *node = NULL;
  void              *data = NULL;

  if (q == NULL) {
    return NULL;
  }
  if (idx >= q->count) {
    return NULL;
  }
  node = q->head;
  while (node != NULL && count != idx) {
    ++count;
    node = node->next;
  }
  data = node->data;
  return data;
}


void *
queuePop (queue_t *q)
{
  void          *data;
  queuenode_t   *node;

  if (q == NULL) {
    return NULL;
  }
  node = q->head;

  data = queueRemove (q, node);
  return data;
}

void *
queueRemoveByIdx (queue_t *q, long idx)
{
  long              count = 0;
  queuenode_t       *node = NULL;
  void              *data = NULL;

  if (q == NULL) {
    return NULL;
  }
  if (idx < 0 || idx >= q->count) {
    return NULL;
  }
  node = q->head;
  while (node != NULL && count != idx) {
    ++count;
    node = node->next;
  }
  if (node != NULL) {
    data = queueRemove (q, node);
  }
  return data;
}

long
queueGetCount (queue_t *q)
{
  if (q != NULL) {
    return q->count;
  }
  return 0;
}

void
queueStartIterator (queue_t *q)
{
  if (q != NULL) {
    q->currentIndex = 0;
    q->iteratorNode = q->head;
    q->currentNode = q->head;
  }
}

void *
queueIterateData (queue_t *q)
{
  void      *data = NULL;

  if (q->iteratorNode != NULL) {
    data = q->iteratorNode->data;
    q->currentIndex++;
    q->currentNode = q->iteratorNode;
    q->iteratorNode = q->iteratorNode->next;
  }
  return data;
}

void *
queueIterateRemoveNode (queue_t *q)
{
  void        *data = NULL;

  if (q == NULL) {
    return NULL;
  }
  data = queueRemove (q, q->currentNode);
  q->currentNode = q->iteratorNode;
  return data;
}

/* internal routines */

static void *
queueRemove (queue_t *q, queuenode_t *node)
{
  void          *data = NULL;

  if (node == NULL) {
    return NULL;
  }
  if (q == NULL) {
    return NULL;
  }
  if (q->head == NULL) {
    return NULL;
  }

  data = node->data;
  if (node->prev == NULL) {
    q->head = node->next;
  } else {
    node->prev->next = node->next;
  }

  if (node->next == NULL) {
    q->tail = node->prev;
  } else {
    node->next->prev = node->prev;
  }
  q->count--;
  free (node);
  return data;
}

