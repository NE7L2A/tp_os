/*
 * hm.c
 *
 *  Created on: 18 mars 2020
 *      Author: napster
 */

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <sys/mman.h>
#include "hm.h"

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif

#define ALIGN 8

struct bloc *cache=NULL;

/* Aligne une taille sur ALIGN (8 octets) */
static size_t align_up(int size){
	if(size<=0) return 0;
	size_t s=(size_t)size;
	return (s+ALIGN-1) & ~((size_t)ALIGN-1);
}

/* Deux blocs sont adjacents si le second commence juste après le premier */
static int is_adjacent(struct bloc *a, struct bloc *b){
	return (char *)a + sizeof(struct bloc) + a->size_to_malloc == (char *)b;
}

/* Ajoute un bloc en fin de liste */
static void append_block(struct bloc *b){
	if(cache==NULL){
		cache=b;
		return;
	}
	struct bloc *cur=cache;
	while(cur->next!=NULL)
		cur=cur->next;
	cur->next=b;
}

void *split(struct bloc *to_split, int size){
	struct bloc *new_bloc=NULL;
	/*
	 * ETAPE 1
	 * On coupe to_split en deux :
	 *   - to_split garde 'size' octets utiles
	 *   - new_bloc recupere le reste (libre)
	 */
	if(to_split==NULL || size<0)
		return NULL;

	/* Il faut au moins la place pour un en-tete + un minimum utilisable */
	if(to_split->size_to_malloc < size + (int)sizeof(struct bloc) + ALIGN)
		return NULL;

	new_bloc=(struct bloc *)((char *)(to_split+1) + size);

	new_bloc->size_to_malloc = to_split->size_to_malloc - size - (int)sizeof(struct bloc);
	new_bloc->state = 0;
	new_bloc->next = to_split->next;

	to_split->next = new_bloc;
	to_split->size_to_malloc = size;

	return new_bloc;
}

void try_to_merge_with_next(struct bloc *to_merge){
	/*
	 * A compléter
	 * Si le bloc suivant est libre et adjacent, on fusionne.
	 */
	if(to_merge==NULL || to_merge->next==NULL)
		return;

	struct bloc *next=to_merge->next;

	if(next->state==0 && is_adjacent(to_merge, next)){
		to_merge->size_to_malloc += (int)sizeof(struct bloc) + next->size_to_malloc;
		to_merge->next = next->next;
	}
}

/*
 * Pour vous donner une idée, vous avez ceci
 */
void init_heap(){
	cache=sbrk(0);
	if(cache==(void *)-1){
		cache=NULL;
		return;
	}

	void *res=sbrk(INITIAL_HEAP_SIZE);
	if(res==(void *)-1){
		cache=NULL;
		return;
	}

	cache->next=NULL;
	cache->size_to_malloc=INITIAL_HEAP_SIZE-sizeof(struct bloc);
	cache->state=0;
}

/* Libere les blocs libres situes en fin de heap (au dessus du break) */
static void release_tail(){
	int changed=1;

	while(changed && cache!=NULL){
		changed=0;
		void *brk=sbrk(0);
		if(brk==(void *)-1)
			return;

		struct bloc *prev=NULL;
		struct bloc *cur=cache;

		while(cur!=NULL){
			if(cur->state==0 &&
			   (char *)cur + sizeof(struct bloc) + cur->size_to_malloc == (char *)brk){

				if(prev!=NULL)
					prev->next=cur->next;
				else
					cache=cur->next;

				sbrk(-(intptr_t)(sizeof(struct bloc) + cur->size_to_malloc));
				changed=1;
				break;
			}

			prev=cur;
			cur=cur->next;
		}
	}
}

void *__malloc(int size){
	if(size<0)
		return NULL;

	if(cache==NULL)
		init_heap();

	if(cache==NULL)
		return NULL;

	size_t need=align_up(size);
	if(need==0)
		need=ALIGN;

	/*
	 * ETAPE 1
	 * Parcourir cache
	 * First fit : premier bloc libre assez grand
	 */
	struct bloc *cur=cache;
	while(cur!=NULL){
		if(cur->state==0 && (size_t)cur->size_to_malloc >= need){
			/* Decouper si le reste est suffisant */
			if(cur->size_to_malloc >= (int)(need + sizeof(struct bloc) + ALIGN)){
				split(cur, (int)need);
			}
			cur->state=1;
			return (void *)(cur+1);
		}
		cur=cur->next;
	}

	/* Aucun bloc libre trouvé : demander à l'OS */
	if(need >= MMAP_THRESHOLD){
		/* Gros bloc via mmap */
		size_t total=sizeof(struct bloc)+need;
		void *mem=mmap(NULL, total, PROT_READ|PROT_WRITE,
		               MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
		if(mem==MAP_FAILED)
			return NULL;

		struct bloc *nb=(struct bloc *)mem;
		nb->size_to_malloc=(int)need;
		nb->state=2;      /* 2 = bloc mmap */
		nb->next=NULL;
		append_block(nb);

		return (void *)(nb+1);
	}else{
		/* Petit bloc via brk/sbrk */
		size_t total=sizeof(struct bloc)+need;
		void *old=sbrk(total);
		if(old==(void *)-1)
			return NULL;

		struct bloc *nb=(struct bloc *)old;
		nb->size_to_malloc=(int)need;
		nb->state=1;
		nb->next=NULL;
		append_block(nb);

		return (void *)(nb+1);
	}
}

void __free(void *ptr){
	/*
	 * Parcourir et rechercher le bloc correspondant à ptr
	 * Coller ce bloc avec les suivants et précédents
	 * ETAPE 2
	 */
	if(ptr==NULL)
		return;

	struct bloc *target=NULL;
	struct bloc *prev=NULL;
	struct bloc *cur=cache;

	while(cur!=NULL){
		if((void *)(cur+1)==ptr){
			target=cur;
			break;
		}
		prev=cur;
		cur=cur->next;
	}

	if(target==NULL)
		return;

	/* Cas d'un bloc mmap : on rend directement a l'OS */
	if(target->state==2){
		if(prev!=NULL)
			prev->next=target->next;
		else
			cache=target->next;

		munmap(target, sizeof(struct bloc)+target->size_to_malloc);
		return;
	}

	/* Marquer le bloc comme libre */
	target->state=0;

	/* Fusion avec le suivant */
	while(target->next!=NULL &&
	      target->next->state==0 &&
	      is_adjacent(target, target->next)){
		try_to_merge_with_next(target);
	}

	/* Fusion avec le precedent */
	prev=NULL;
	cur=cache;
	while(cur!=NULL && cur!=target){
		prev=cur;
		cur=cur->next;
	}

	if(prev!=NULL && prev->state==0 && is_adjacent(prev, target)){
		prev->size_to_malloc += (int)sizeof(struct bloc) + target->size_to_malloc;
		prev->next = target->next;
		target = prev;

		while(target->next!=NULL &&
		      target->next->state==0 &&
		      is_adjacent(target, target->next)){
			try_to_merge_with_next(target);
		}
	}

	/* Rendre la memoire a l'OS si possible */
	release_tail();
}

void *__realloc(void *ptr, int size){
	/*
	 * ETAPE 3
	 */
	if(ptr==NULL)
		return __malloc(size);

	if(size<=0){
		__free(ptr);
		return NULL;
	}

	/* Retrouver le bloc correspondant a ptr */
	struct bloc *target=NULL;
	struct bloc *cur=cache;

	while(cur!=NULL){
		if((void *)(cur+1)==ptr){
			target=cur;
			break;
		}
		cur=cur->next;
	}

	if(target==NULL)
		return NULL;

	size_t need=align_up(size);
	if(need==0)
		need=ALIGN;

	/* Cas 1 : le bloc actuel est deja assez grand */
	if((size_t)target->size_to_malloc >= need){
		if(target->size_to_malloc >= (int)(need + sizeof(struct bloc) + ALIGN)){
			split(target, (int)need);
		}
		return ptr;
	}

	/* Cas 2 : essayer de fusionner avec les suivants libres */
	while(target->next!=NULL &&
	      target->next->state==0 &&
	      is_adjacent(target, target->next)){
		try_to_merge_with_next(target);
	}

	if((size_t)target->size_to_malloc >= need){
		if(target->size_to_malloc >= (int)(need + sizeof(struct bloc) + ALIGN)){
			split(target, (int)need);
		}
		return ptr;
	}

	/* Cas 3 : nouvelle allocation + copie + liberer l'ancien */
	void *new_ptr=__malloc(size);
	if(new_ptr==NULL)
		return NULL;

	size_t copy_size = target->size_to_malloc < size ? target->size_to_malloc : size;
	memcpy(new_ptr, ptr, copy_size);
	__free(ptr);

	return new_ptr;
}
