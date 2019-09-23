/*
 *  Progetto Algoritmi e Principi dell'Informatica - 2019
 *
 *	Author: Davide Merli
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>

#define HASH_DIMENSION 10000

typedef struct list List;
typedef struct tree_t Tree;

/*-------------
 * Hash table *
 *-------------
 *
 * Collisions are handled through chaining,
 * the main array is initialized with a size of 10000 entries (HASH_DIMENSION)
 */
typedef struct entry_t {
	char 			*id;		//Entity ID
	struct entry_t 		*next;		//Next element in the chain
	List 			*rel_list;	//List of relation types, storing trees with the actual relation nodes
} entity_t;

typedef struct {
	entity_t 		**table;
} HashTable;

/*------------------
 * Red Black Tree  *
 *------------------
 *
 * Every entity has a list of trees (one for every relation type)
 *
 * The tree contains the relations that have the current entity has the
 * "to" of the relation
 *
 * The relation is added in 'addrel' with the format:
 * addrel "from" "to" "type
 *
 * In the nodes, the entities are saved as pointers; in a 64bit architecture,
 * this saves memory if the entities have IDs longer than 8 chars, since
 * a single pointer has a size of 8bytes, saving the actual string only once
 * in the hash table with all the current entities that are being monitored.
 */
typedef struct node {
	entity_t 		*to;				//Entity pointer
	char 			color;				//Color of the node, used for 'rb_insert_fixup' and 'rb_delete_fixup'
	struct node 		*p, *left, *right;		//Pointers to the parent, the left child and the right child
} node;

struct tree_t {
	node 			*root;	//Root of the tree. This is the only node with the parent being NIL

	short unsigned int 	size;	//Number of nodes, modified in rb_insert & rb_delete, initialized as 0 in 'init_tree'
};

/*
 *	Possible rb trees node colors
 */
typedef enum {RED, BLACK} Color;

/*--------------
 * Linked list *
 *--------------
 *
 * These are used for saving the relation types
 * Using RB trees for these would be overkill since it is specified that
 * the number of relation types is very low, so the complexity of
 * iterating types can be assumed as constant.
 *
 * Furthermore, using linked list helps with code clarity and
 * readability instead of having trees inside trees
 *
 *
 * The tree is used to store "from" Entries of the 'key' relation type,
 * as well as to store the data to print when the function 'report' is called.
 *
 * The trees used to store data for 'report' are the ones in 'RELATION_TYPES' list.
 */
typedef struct list_t { //Node of the list
	char 			*key;				//Relation type name
	struct list_t 		*next;				//Next element in the list
	Tree 			*tree; 				//The tree containing entities relations towards one single entity
	short unsigned int 	current_maximum;		//The value of the maximum number of relation, it is printed for every relation type report
} list_t;

struct list { //The struct containing the head of the list
	list_t 			*head;				//Head of the list
};

/*
 * Global variable for the entities hashtable
 */
HashTable 	*ENTITIES;

/*
 * List of relation types, the one used to store data for reporting.
 * Updated every 'addrel', 'delrel', 'delent', when necessary
 */
List 		*RELATION_TYPES;

/*
 * NIL node for the RB trees, used as a global variable for code semplicity
 */
node 		*NIL;

/*--------------------------------------------*/
/*			Needed function prototypes		  */
/*--------------------------------------------*/

int 		fputs_unlocked(const char *, FILE *);

node 		*init_NIL(void);
List 		*init_list(void);
Tree 		*init_tree(void);
HashTable 	*init_table(void);

list_t 		*list_search(List *, char *);
node 		*tree_search(node *, entity_t *);
entity_t 	*hash_search(HashTable *, char *);

void 		clear_list(List *);
void 		clear_tree(Tree *, node *, bool);
void 		clear_hash_table(HashTable *);

list_t 		*list_insert(List *, char *);
list_t 		*list_insert_unordered(List *, char *);
node 		*rb_insert(Tree *, entity_t *);
int 		hash_insert(HashTable *, char *);

void 		list_delete(List *, char *);
void 		rb_delete(Tree *, node *);
int 		hash_delete(HashTable *, char *);

bool 		print_relation_tree(node *);
void 		restore_data_maximum(list_t *, char *);

void 		process_input(FILE *);
void 		print_string(char *);

/*--------------------------------------------*/

/*
 * MAIN

 * The main method of the program
 */
int main(void) {
	//Initializes the NIL node
	NIL = init_NIL();
	//Initializes the Hash Table
	ENTITIES = init_table();
	//Initializes the head of the relation type list
	RELATION_TYPES = init_list();

	//Processes all the input from stdin
	process_input(stdin);

	//When every command has been executed, frees all the memory
	//Frees all the nodes of the 'RELATION_TYPES' list
	clear_list(RELATION_TYPES);
	free(RELATION_TYPES);

	//Frees all memory allocated for relations and Entries
	clear_hash_table(ENTITIES);
	free(ENTITIES);

	free(NIL);

	return 0;
}

/************************/
/*		COMMANDS 		*/
/************************/

/*
 * ADDENT command
 *
 * Searches if the given entity is already present in the hashtable,
 * if not, inserts it
 */
void addent(char *ident) {
	entity_t *search = hash_search(ENTITIES, ident);

	if (search == NULL) {
		hash_insert(ENTITIES, ident);
	}
}

/*
 * ADDREL command
 *
 * After checking if 'from' and 'to' entities exist, and the relation does not exist already,
 * gets the Tree of the entity corresponding to 'to', and add a node with the entity_t
 * 'from'.
 *
 * After insertion adds a name to the report data tree or, if the current maximum value
 * is overridden, clears the data tree and inserts the current 'to' entity_t
 */
void addrel(char *from, char *to, char *type) {
	entity_t *from_entity = hash_search(ENTITIES, from);
	entity_t *to_entity = hash_search(ENTITIES, to);

	//Exits if one the entities is not found.
	if (from_entity == NULL || to_entity == NULL) return;

	//The node of the list containing the current 'type' relation data
	list_t *data_list = list_search(RELATION_TYPES, type);

	//Gets the data_list or if not already present adds it to the list for reporting
	if (data_list == NULL) {
		data_list = list_insert(RELATION_TYPES, type);
	}

	//The node of the 'to' Entry with the current relation type
	list_t *rel_list = list_search(to_entity->rel_list, type);

	//Gets the list node or adds it if not already present in the entity relation types
	if (rel_list == NULL) {
		rel_list = list_insert_unordered(to_entity->rel_list, type);
	}

	//Searches if the relation is already present, if not inserts it
	if (tree_search(rel_list->tree->root, from_entity) == NIL) {
		rb_insert(rel_list->tree, from_entity);
	}

	//If the number of relations that point to 'to' is equal to the current maximum of this type of relation,
	//adds the entity to the report list
	if (rel_list->tree->size == data_list->current_maximum) {
		if (tree_search(data_list->tree->root, to_entity) == NIL) {
			rb_insert(data_list->tree, to_entity);
		}
		//Overrides the data tree if the number of relations is greater than current maximum
	} else if (rel_list->tree->size > data_list->current_maximum) {
		clear_tree(data_list->tree, data_list->tree->root, true);

		rb_insert(data_list->tree, to_entity);
		data_list->current_maximum = rel_list->tree->size;
	}
}

/*
 * DELREL command
 *
 * After checking if 'from' and 'to' entities exist, and if the relation does exist,
 * gets the Tree of the entity corresponding to 'to', and deletes the node
 * with the entity_t 'from'.
 *
 * After insertion adds a name to the report data tree or, if the current maximum value
 * is overridden, clears the data tree and inserts the current 'to' entity_t
 */
void delrel(char *from, char *to, char *type) {
	entity_t *from_entity = hash_search(ENTITIES, from);
	entity_t *to_entity = hash_search(ENTITIES, to);

	//Checks if any of the given entities does not exists
	if (from_entity == NULL || to_entity == NULL) return;

	//The data list with 'type'
	list_t *data_list = list_search(RELATION_TYPES, type);

	//Returns if 'type' of relation is not present globally
	if (data_list == NULL) return;

	//Relation list of the entity_t 'to'
	list_t *rel_list = list_search(to_entity->rel_list, type);

	//Returns if 'type' of relation is not present in the entity_t
	if (rel_list == NULL) return;

	//The node to delete
	node *to_delete = tree_search(rel_list->tree->root, from_entity);

	//Returns if the node is not found (relation not present)
	if (to_delete == NIL) return;

	//Deletes the node
	rb_delete(rel_list->tree, to_delete);

	//Checks if the data tree needs to be rewritten (meaning the current relation had 'size' equal to current maximum)
	if (rel_list->tree->size + 1 == data_list->current_maximum) {
		//Case there is more than one entity with the size equal to current maximum
		//Only deletes the node from the data tree
		if (data_list->tree->size > 1) {
			rb_delete(data_list->tree, tree_search(data_list->tree->root, to_entity));
			//Otherwise calls the function 'restore_data_maximum' that rewrites the data tree
		} else {
			restore_data_maximum(data_list, type);
		}
	}
}

/*
 * DELENT command
 *
 * After checking if the given entities exist, deletes it
 * from the hashtable, then deletes all the relations it had
 * with other entities. Finally deletes the relations the other
 * entities had with the given entity.
 */
void delent(char *ident) {
	entity_t 	*search = hash_search(ENTITIES, ident);
	entity_t 	*ent_cursor;

	node 		*deletion;
	list_t 		*rel_cursor, *list, *next;

	//Returns if entity is not present
	if (search == NULL) return;

	//Head of the list of the global relation types
	rel_cursor = RELATION_TYPES->head;

	while (rel_cursor != NULL) {
		//Saves the next incase rel_cursor needs to be removed (no more relations with that type)
		next = rel_cursor->next;

		//Cicles the entities
		for (int i = 0; i < HASH_DIMENSION; i++) {
			ent_cursor = ENTITIES->table[i];

			while (ent_cursor != NULL) {
				//Gets the relation type
				list = list_search(ent_cursor->rel_list, rel_cursor->key);

				//If relation type list is not present skips to the next entity_t
				if (list == NULL) {
					ent_cursor = ent_cursor->next;
					continue;
				}

				//If the entity_t is the one to be deleted, completely wipes the relations from the tree
				if (ent_cursor == search) {
					clear_tree(list->tree, list->tree->root, true);

					if ((deletion = tree_search(rel_cursor->tree->root, ent_cursor)) != NIL) {
						rb_delete(rel_cursor->tree, deletion);
					}
					//Otherwise searches if the relation with the entity to delete is present, and deletes it
				} else if ((deletion = tree_search(list->tree->root, search)) != NIL) {
					rb_delete(list->tree, deletion);
				}

				ent_cursor = ent_cursor->next;
			}
		}

		//Restores the correct data tree information
		restore_data_maximum(rel_cursor, rel_cursor->key);

		rel_cursor = next;
	}

	//Finally, deletes the entity_t
	hash_delete(ENTITIES, ident);
}

/*
 * REPORT command
 *
 * Simply prints out the information stored by the other commands in the 'RELATION_TYPES' list
 *
 * Uses fputs since it's faster than printf when formatting is not necessary
 */
void report(void) {
	list_t *rel_cursor = RELATION_TYPES->head;

	//If nothing has to be printed, prints out none
	if (rel_cursor == NULL) {
		fputs_unlocked("none", stdout);
	} else {
		while (rel_cursor != NULL) {
			//Prints relation type
			print_string(rel_cursor->key);

			//Prints all the entities
			print_relation_tree(rel_cursor->tree->root);

			//Prints the value maximum
			printf("%d; ", rel_cursor->current_maximum);

			rel_cursor = rel_cursor->next;
		}
	}

	fputc_unlocked('\n', stdout);
}

/*
 * Given a data list and a 'type',
 * Checks all the entities to get the new maximum
 *
 * Used to restore the data tree for 'report',
 */
void restore_data_maximum(list_t *data_list, char *type) {
	list_t 		*rel_list;
	entity_t 	*ent_cursor;

	//Re-initializes the current maximum to 0
	data_list->current_maximum = 0;

	//Cicles all the entities in the hashtable
	for (int i = 0; i < HASH_DIMENSION; i++) {
		ent_cursor = ENTITIES->table[i];

		while (ent_cursor != NULL) {
			//entity_t 'type' relation list
			rel_list = list_search(ent_cursor->rel_list, type);

			if (rel_list != NULL) {
				//Case size is equal to maximum, only adds a node to the data list
				if (rel_list->tree->size > 0 && rel_list->tree->size == data_list->current_maximum) {
					rb_insert(data_list->tree, ent_cursor);
					//Case size is greater than maximum, clears the data tree, and inserts first node
				} else if (rel_list->tree->size > data_list->current_maximum) {
					clear_tree(data_list->tree, data_list->tree->root, true);

					//Sets the new maximum
					data_list->current_maximum = rel_list->tree->size;
					rb_insert(data_list->tree, ent_cursor);
				}
			}

			//Next element if collisions in hashtable are present
			ent_cursor = ent_cursor->next;
		}
	}

	//If no relations are found at all, deletes the data tree and the relation type
	if (data_list->current_maximum == 0) {
		clear_tree(data_list->tree, data_list->tree->root, true);
		list_delete(RELATION_TYPES, type);
	}
}

/****************************/
/*	INPUT FUNCTIONS     */
/****************************/

/*
 * Given four arguments, checks the 'command' parameter and calls
 * the right command
 *
 * Returns -1 if 'end' is called or a not recognised command is found
*/
int process_arguments(char *command, char *arg1, char *arg2, char *arg3) {
	if (strcmp(command, "addent") == 0) {
		addent(arg1);
		return 0;
	} else if (strcmp(command, "delent") == 0) {
		delent(arg1);
		return 1;
	} else if (strcmp(command, "addrel") == 0) {
		addrel(arg1, arg2, arg3);
		return 2;
	} else if (strcmp(command, "delrel") == 0) {
		delrel(arg1, arg2, arg3);
		return 3;
	} else if (strcmp(command, "report") == 0) {
		report();
		return 4;
	} else if (strcmp(command, "end") == 0) {
		return -1;
	} else {
		return -1;
	}
}

/*
 * Gets stdin input until 'end' command is encountered
 */
void process_input(FILE *input) {
	//Arguments
	char 		arguments[4][100], current_char;

	//Counters
	int 		char_count = 0, arg_count = 0, code;

	while ((current_char = getc_unlocked(input)) != EOF) {
		//When a space or a new line is found, needs to allocate another argument`
		//If new line is read, processes the command
		if (current_char == ' ' || current_char == '\n') {

			//Sets the end of the string
			arguments[arg_count][char_count] = '\0';

			//Increments 'arg_count' since the previous one has been allocated
			arg_count++;

			//Calls the process function if new line is found
			if (current_char == '\n') {
				code = process_arguments(arguments[0], arguments[1], arguments[2], arguments[3]);

				//Exits if end is found
				if (code == -1) return;

				arg_count = 0;
			}

			//Restores the initial counter for the buffer
			char_count = 0;
		} else if (current_char != '\"') { //Otherwise stores the characters in the buffer string
			arguments[arg_count][char_count] = current_char;

			char_count++;
		}
	}
}

/****************************/
/*	LIST FUNCTIONS	    */
/****************************/

/*
 * Inizializes a new list
 */
List *init_list(void) {
	List *list = malloc(sizeof(List));
	list->head = NULL;

	return list;
}

/*
 * Given a list and a string 'key',
 * returns the list node with the given 'key', NULL otherwise
 */
inline list_t *list_search(List *list, char *key) {
	list_t *cursor = list->head;

	//Exits when found or NULL
	while (cursor != NULL && strcmp(cursor->key, key) != 0) {
		cursor = cursor->next;
	}

	return cursor;
}

list_t *list_insert_unordered(List *list, char *key) {
	//Creates and initializes the node
	list_t 		*new = malloc(sizeof(list_t));
	list_t 		*cursor, *prev;

	new->key = strdup(key);
	new->tree = init_tree();
	new->current_maximum = 0;
	new->next = list->head;

	list->head = new;

	return new;
}

/*
 * Given a list and a string 'key'
 * inserts the node in the list in alphabetic order
 *
 * Does not check if the given 'key' is already present,
 * so 'list_search' needs to be called first
 */
list_t *list_insert(List *list, char *key) {
	//Creates and initializes the node
	list_t 		*new = malloc(sizeof(list_t));
	list_t 		*cursor, *prev;

	new->key = strdup(key);
	new->tree = init_tree();
	new->current_maximum = 0;

	prev = NULL;
	cursor = list->head;

	//Exits when found position or out of the list (tail insertion)
	while (cursor != NULL && strcmp(cursor->key, key) < 0) {
		prev = cursor;
		cursor = cursor->next;
	}

	if (prev == NULL) {//If the element is to be put as the new head
		new->next = list->head;
		list->head = new;
	} else {
		//Node linking
		prev->next = new;
		new->next = cursor;
	}

	return new;
}

/*
 * Given a list and a string 'key'
 * deletes the list node with the given 'key'
 *
 * Needs to be checked beforehand if the 'key' is present in the list
 * with 'list_search'
 */
void list_delete(List *list, char *key) {
	list_t *cursor, *prev, *temp, *todelete;

	if (list->head != NULL && strcmp(list->head->key, key) == 0) {
		//Sets the head of the list to the second element
		temp = list->head;
		list->head = list->head->next;

		todelete = temp;
	} else {
		prev = list->head;
		cursor = list->head->next;

		while (cursor != NULL && strcmp(cursor->key, key) != 0) {
			prev = cursor;
			cursor = cursor->next;
		}

		//cursor is now the element to delete
		prev->next = cursor->next;

		todelete = cursor;
	}

	//Frees all allocated memory
	clear_tree(todelete->tree, todelete->tree->root, true);
	free(todelete->key);
	free(todelete->tree);
	free(todelete);
}

/*
 * Given a list,
 * deletes all nodes and frees the memory
 */
void clear_list(List *list) {
	list_t *cursor = list->head, *temp;

	while (cursor != NULL) {
		//Gets the element to delete and goes further by one element
		temp = cursor;
		cursor = cursor->next;

		//Frees all allocated memory
		clear_tree(temp->tree, temp->tree->root, true);
		free(temp->key);
		free(temp->tree);
		free(temp);
	}
}

/*
 * Prints the given list
 *
 * Only used for debugging
 */
void print_list(List *list) {
	printf("\ndebug list:\n");
	list_t *cursor = list->head;

	while (cursor != NULL) {
		printf("%s\t", cursor->key);

		cursor = cursor->next;
	}
	printf("\n\n");
}

/********************************/
/*		HASH TABLE FUNCTIONS	*/
/********************************/

/*
 * Creates and returns an HashTable
 */
HashTable *init_table(void) {
	HashTable *ht = malloc(sizeof(HashTable));
	ht->table = calloc(HASH_DIMENSION, sizeof(entity_t)); //Sets every cell to NULL
	return ht;
}

/*
 * Given a character shifts and returns its value in a range from '1' to '54'
 *
 * Used as util in the 'hash_string' function
 */
int shift_char_value(char c) {
	if (c == 45) return 1; //The '-' char
	if (c == 95) return 2; //The '_' char

	if (c >= 65 /*A*/ && c <= 90 /*Z*/) return c - 62; //uppercase letters
	if (c >= 97 /*a*/ && c <= 122 /*z*/) return c - 68; //lowercase letters

	return c;
}

/*
 * Given a string
 * returns the index where to put the string into the hashtable,
 * given the hash dimension as a constant
 *
 * Gets the current char value, multipling it by the index of the character, squared
 */
int hash_string(char *to_hash) {
	int 		length = strlen(to_hash);
	long int 	hash = 0;

	//Sums the characters value multiplied by their position in the string
	for (int i = 0; i < length; i++) {
		hash += i * i * shift_char_value(to_hash[i]);
	}

	return hash % HASH_DIMENSION; //Returns an index from '0' to 'HASH_DIMENSION -1'
}

/*
 * Prints the global HashTable
 *
 * Only used for debugging
 */
void print_hash(HashTable *ht) {
	entity_t *current, *cursor;

	for (int i = 0; i < HASH_DIMENSION; i++) {
		current = ht->table[i];
		if (current == NULL) continue;

		printf("%d: \t\t", i);

		cursor = current;
		while (cursor != NULL) {
			printf("%s\t", cursor->id);
			cursor = cursor->next;
		}

		printf("\n");
	}
}

/*
 * Given a string,
 * creates a new entity_t, and puts it into the global HashTable
 * returns the index
 */
int hash_insert(HashTable *ht, char *to_hash) {
	//Gets the hashtable index and the head of the collision list
	int 		index = hash_string(to_hash);
	entity_t 	*head = ht->table[index];

	//Allocs memory for the new node and initializes the variables
	entity_t 	*new = malloc(sizeof(entity_t));

	new->id = strdup(to_hash);
	new->rel_list = init_list();
	new->next = head; //Links head to 'next'

	//Head insertion
	ht->table[index] = new;

	return index;
}

/*
 * Given a string
 * returns the corresponding entity_t from the global HashTable, NULL if not present
 */
entity_t *hash_search(HashTable *ht, char *to_hash) {
	//Gets the index where the entity_t should be
	int 		index = hash_string(to_hash);

	entity_t 	*head = ht->table[index];
	entity_t	*cursor = head;

	//Cicles the 'collisions list'
	while (cursor != NULL && strcmp(cursor->id, to_hash) != 0) {
		cursor = cursor->next;
	}

	//At this point the cursor value is either the searched entity_t or NULL
	return cursor;
}

/*
 * Given a string,
 * deletes the entity_t corresponding to that string
 *
 * returns the index where the entity_t was
 *
 * Needs to be checked beforehand with 'hash_search' if the entity_t is effectively present
 */
int hash_delete(HashTable *ht, char *to_hash) {
	//Index of the line where the node should be
	int 		index = hash_string(to_hash);

	entity_t 	*head = ht->table[index], *todelete;
	entity_t 	*cursor, *prev;

	//if the list empty (not actually possible since used after 'hash_search')
	if (head == NULL) return -1;

	//if the head is the element to remove
	if (head != NULL && strcmp(head->id, to_hash) == 0) {
		ht->table[index] = head->next;

		todelete = head;
	} else {
		//Cicles the list and stops when found or NULL
		cursor = head->next;
		prev = head;

		while (cursor != NULL && strcmp(cursor->id, to_hash) != 0) {
			prev = cursor;
			cursor = cursor->next;
		}

		if (cursor == NULL) return -1;

		//Links the nodes
		prev->next = cursor->next;

		todelete = cursor;
	}

	//Frees all memory
	clear_list(todelete->rel_list);
	free(todelete->rel_list);
	free(todelete->id);
	free(todelete);

	return index;
}

/*
 * Iteratively frees every memory allocated in the hash table entries
 */
void clear_hash_table(HashTable *ht) {
	entity_t *cursor, *temp;

	for (int i = 0; i < HASH_DIMENSION; i++) {
		cursor = ht->table[i];

		while (cursor != NULL) {
			temp = cursor;
			cursor = cursor->next;

			clear_list(temp->rel_list);
			free(temp->rel_list);
			free(temp->id);
			free(temp);
		}
	}
}

/************************/
/*		RB FUNCTIONS	*/
/************************/

/*
 * Given an entity_t,
 * allocates memory for a new node and returns it
 */
node *init_node(entity_t *to) {
	node *z = malloc(sizeof(node));

	//inserts arguments
	z->to = to;
	z->left = NIL;
	z->right = NIL;
	z->color = RED;

	return z;
}

/*
 * Util function to initialize the NIL node
 */
node *init_NIL(void) {
	node *new = malloc(sizeof(node));
	new->p = new;
	new->right = NIL;
	new->left = NIL;
	new->color = BLACK;

	return new;
}

/*
 * Given a node,
 * returns the minimum
 */
node *tree_min(node *x) {
	while (x->left != NIL)
		x = x->left;

	return x;
}

/*
 * Given a node,
 * returns the maximum
 */
node *tree_max(node *x) {
	while (x->right != NIL)
		x = x->right;

	return x;
}

/*
 * Given a node,
 * returns the successor in the Tree
 */
node *tree_successor(node *x) {
	if (x->right != NIL)
		return tree_min(x->right);

	node *y = x->p;

	while (y != NIL && x == y->right) {
		x = y;
		y = y->p;
	}

	return y;
}

/*
 * Recursively frees in post-order all the nodes of the given tree
 * 'first' is used to reinitialize the tree only once
 */
void clear_tree(Tree *tree, node *root, bool first) {
	if (root != NIL) {
		clear_tree(tree, root->left, false);
		clear_tree(tree, root->right, false);

		free(root);

		//Executed once thanks to 'first' parameter
		if (first) {
			tree->root = NIL;
			tree->size = 0;
		}
	}
}

/*
 * Given a Tree and a node,
 * performs a RB-Tree left rotation
 */
void left_rotate(Tree *tree, node *x) {
	node *y;

	y = x->right;
	x->right = y->left;

	if (y->left != NIL) {
		y->left->p = x;
	}

	y->p = x->p;

	if (x->p == NIL) {
		tree->root = y;
	} else if (x == x->p->left) {
		x->p->left = y;
	} else {
		x->p->right = y;
	}

	y->left = x;
	x->p = y;
}

/*
 * Given a Tree and a node,
 * performs a RB-Tree right rotation
 */
void right_rotate(Tree *tree, node *x) {
	node *y;

	y = x->left;
	x->left = y->right;

	if (y->right != NIL) {
		y->right->p = x;
	}

	y->p = x->p;
	if (x->p == NIL) {
		tree->root = y;
	} else if (x == x->p->right) {
		x->p->right = y;
	} else {
		x->p->left = y;
	}

	y->right = x;
	x->p = y;
}

/*
 * Given a tree and a node,
 * rebalances the RB-Tree after insertion
 */
void rb_insert_fixup(Tree *tree, node *z) {
	node *y;

	while (z->p->color == RED) {
		if (z->p == z->p->p->left) {
			y = z->p->p->right;

			if (y->color == RED) {
				//Case 1
				z->p->color = BLACK;
				y->color = BLACK;
				z->p->p->color = RED;
				z = z->p->p;
			} else if (z == z->p->right) {
				//Case 2
				z = z->p;
				left_rotate(tree, z);
			} else {
				//Case 3
				z->p->color = BLACK;
				z->p->p->color = RED;
				right_rotate(tree, z->p->p);
			}
		} else {
			y = z->p->p->left;

			if (y->color == RED) {
				//Case 1
				z->p->color = BLACK;
				y->color = BLACK;
				z->p->p->color = RED;
				z = z->p->p;
			} else if (z == z->p->left) {
				//Case 2
				z = z->p;
				right_rotate(tree, z);
			} else {
				//Case 3
				z->p->color = BLACK;
				z->p->p->color = RED;
				left_rotate(tree, z->p->p);
			}
		}
	}

	tree->root->color = BLACK;
}

/*
 * Given a tree and a node,
 * rebalances the RB-Tree after deletion
 */
void rb_delete_fixup(Tree *tree, node *x) {
	node *w = NIL;

	while (x != tree->root && x->color == BLACK) {
		if (x == x->p->left) {
			w = x->p->right;

			if (w->color == RED) {
				w->color = BLACK;
				x->p->color = RED;
				left_rotate(tree, x->p);
				w = x->p->right;
			}

			if (w->left->color == BLACK && w->right->color == BLACK) {
				//Case 1
				w->color = RED;
				x = x->p;
			} else if (w->right->color == BLACK) {
				//Case 2
				w->left->color = BLACK;
				w->color = RED;
				right_rotate(tree, w);
				w = x->p->right;
			} else {
				//Case 3
				w->color = x->p->color;
				x->p->color = BLACK;
				w->right->color = BLACK;
				left_rotate(tree, x->p);
				x = tree->root;
			}
		} else {
			w = x->p->left;

			if (w->color == RED) {
				w->color = BLACK;
				x->p->color = RED;
				right_rotate(tree, x->p);
				w = x->p->left;
			}

			if (w->right->color == BLACK && w->left->color == BLACK) {
				//Case 1
				w->color = RED;
				x = x->p;
			} else if (w->left->color == BLACK) {
				//Case 2
				w->right->color = BLACK;
				w->color = RED;
				left_rotate(tree, w);
				w = x->p->left;
			} else {
				//Case 3
				w->color = x->p->color;
				x->p->color = BLACK;
				w->left->color = BLACK;
				right_rotate(tree, x->p);
				x = tree->root;
			}
		}
	}

	x->color = BLACK;
}

/*
 * Given a Tree and an entity_t,
 * creates a new node, inserts it into the tree and returns the node itself
 *
 * If necessary, rebalances the tree with 'rb_insert_fixup'
 */
node *rb_insert(Tree *tree, entity_t *to) {
	node 	*x, *y;
	node 	*z = init_node(to);

	y = NIL;
	x = tree->root;

	while (x != NIL) {
		y = x;

		//Goes left or right checking alphabetic order
		if (strcmp(z->to->id, x->to->id) < 0) {
			x = x->left;
		} else {
			x = x->right;
		}
	}

	z->p = y;

	if (y == NIL) {
		tree->root = z;
		tree->root->color = BLACK;
	} else {

		//Inserts left or right checking alphabetic order
		if (strcmp(z->to->id, y->to->id) < 0)
			y->left = z;
		else
			y->right = z;
	}

	//Increments tree size
	tree->size = tree->size + 1;

	//Rebalances the Tree
	rb_insert_fixup(tree, z);

	return z;
}

/*
 * Given a Tree and a node,
 * deletes the given node
 */
void rb_delete(Tree *tree, node *z) {
	node *x, *y;

	if (z->left == NIL || z->right == NIL) {
		y = z;
	} else {
		y = tree_successor(z);
	}

	if (y->left != NIL) {
		x = y->left;
	} else {
		x = y->right;
	}

	x->p = y->p;

	if (y->p == NIL) {
		tree->root = x;
	} else if (y == y->p->left) {
		y->p->left = x;
	} else {
		y->p->right = x;
	}

	if (y != z) {
		z->to = y->to;
	}

	//Rebalances the Tree if needed
	if (y->color == BLACK) {
		rb_delete_fixup(tree, x);
	}

	//Decrements the size of the Tree
	tree->size = tree->size - 1;

	free(y);
}

/*
 * Given a node (root) and an entity_t,
 * recursively returns the corresponding node if present, NIL otherwise
 */
node *tree_search(node *x, entity_t *to) {
	//Case Tree is empty or entity_t is NULL
	if (x == NIL || to == NULL) return x;

	int 	compare = strcmp(to->id, x->to->id);

	node 	*toReturn;

	//Case found
	if (compare == 0) {
		toReturn = x;
	} else if (compare < 0) { //Left or right otherwise
		toReturn = tree_search(x->left, to);
	} else {
		toReturn = tree_search(x->right, to);
	}

	return toReturn;
}

/*
 * Util function to initialize a Tree
 */
Tree *init_tree(void) {
	Tree *tree = malloc(sizeof(Tree));
	tree->root = NIL;
	tree->size = 0;

	return tree;
}

/*
 * Given a node (root), recursively prints the keys,
 * returns TRUE if has printed anything, FALSE otherwise
 */
bool print_relation_tree(node *root) {
	if (root != NIL) {
		print_relation_tree(root->left);

		print_string(root->to->id);

		print_relation_tree(root->right);

		return true;
	}

	return false;
}

/*
 * Prints a given Tree
 *
 * Only used for debugging, space is set to 0 on the first call
 */
void print_tree(node * root, int space) {
	if (root == NIL) return;

	space += 20;

	if (root->right != NIL)
		print_tree(root->right, space);

	printf("\n");

	for (int i = 20; i < space; i++)
		printf(" ");

	printf("%s\n", root->to->id);

	if (root->left != NIL)
		print_tree(root->left, space);
}

/*
 * Prints a given string adding double quotes and a space after it
 */
void print_string(char *string) {
	int length = strlen(string);

	//Using fputc since it's faster than printf
	fputc_unlocked('\"', stdout);

	for (int i = 0; i < length; i++) {
		fputc_unlocked(string[i], stdout);
	}

	fputc_unlocked('\"', stdout);
	fputc_unlocked(' ', stdout);
}
