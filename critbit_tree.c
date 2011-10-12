#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/*
 * The address of a block returned by malloc or realloc in the GNU system is
 * always a multiple of eight (or sixteen on 64-bit systems). But if you need
 * to use posix_memalign() instead, then uncomment:
 *
 * #define USE_POSIX_MEMALIGN
 */

#define IS_INTERNAL(node)          ((uintptr_t) node & 1)
#define GENERATE_DIR(bitmask, val) (((bitmask | val) + 1) >> 8)

typedef enum {CBTREE_OK, CBTREE_FAIL, CBTREE_ENOMEM} cbtree_result_t;

typedef struct {
	void    *child[2];
	uint32_t byte;
	uint8_t  bitmask;
} critbit_node_t;

typedef struct {
	void *root;
} critbit_tree_t;

static bool cbtree_contains_str(critbit_tree_t *tree, const char *str);
static bool cbtree_contains(critbit_tree_t *tree, const uint8_t *data, size_t len);
static bool cbtree_contains_generic(critbit_tree_t *tree, const uint8_t *data, size_t len, bool is_str);
static char *cbtree_find_nearest_str(critbit_tree_t *tree, const char *str);
static uint8_t *cbtree_find_nearest(critbit_tree_t *tree, const uint8_t *data, size_t len);
static uint8_t *cbtree_find_nearest_generic(critbit_tree_t *tree, const uint8_t *data, size_t len, bool is_str, uint32_t *next_byte);
static cbtree_result_t cbtree_insert_str(critbit_tree_t *tree, const char *str);
static cbtree_result_t cbtree_insert(critbit_tree_t *tree, const uint8_t *data, size_t len);
static cbtree_result_t cbtree_insert_generic(critbit_tree_t *tree, const uint8_t *data, size_t len, bool is_str);
static void *cbtree_allocate(size_t size);

static bool cbtree_contains_str(critbit_tree_t *tree, const char *str)
{
	return cbtree_contains_generic(tree, (uint8_t *) str, strlen(str), true);
}

static bool cbtree_contains(critbit_tree_t *tree, const uint8_t *data, size_t len)
{
	return cbtree_contains_generic(tree, data, len, false);
}

static bool cbtree_contains_generic(critbit_tree_t *tree, const uint8_t *data, size_t len, bool is_str)
{
	uint8_t *node;
	uint32_t next_byte;

	if (!tree->root)
		return false;

	node = cbtree_find_nearest_generic(tree, data, len, is_str, &next_byte);
	if (is_str)
		return strcmp((char *) data + next_byte, (char *) node + next_byte) == 0;
	else
		return memcmp(data + next_byte, node + next_byte, len - next_byte) == 0;
}

static char *cbtree_find_nearest_str(critbit_tree_t *tree, const char *str)
{
	uint32_t next_byte;

	return (char *) cbtree_find_nearest_generic(tree, (uint8_t *) str, strlen(str), true, &next_byte);
}

static uint8_t *cbtree_find_nearest(critbit_tree_t *tree, const uint8_t *data, size_t len)
{
	uint32_t next_byte;

	return cbtree_find_nearest_generic(tree, data, len, false, &next_byte);
}

static uint8_t *cbtree_find_nearest_generic(critbit_tree_t *tree, const uint8_t *data, size_t len, bool is_str, uint32_t *next_byte)
{
	critbit_node_t *node = tree->root;

	*next_byte = 0;
	while (IS_INTERNAL(node)) {
		critbit_node_t *int_node = (critbit_node_t *) ((uintptr_t) node - 1);
		uint8_t         val = 0;
		int             dir;

		if (int_node->byte < len) {
			val = data[int_node->byte];
			*next_byte = int_node->byte + 1;
		}
		dir = GENERATE_DIR(int_node->bitmask, val);
		node = int_node->child[dir];
	}

	return (uint8_t *) node;
}

static cbtree_result_t cbtree_insert_str(critbit_tree_t *tree, const char *str)
{
	return cbtree_insert_generic(tree, (uint8_t *) str, strlen(str), true);
}

static cbtree_result_t cbtree_insert(critbit_tree_t *tree, const uint8_t *data, size_t len)
{
	return cbtree_insert_generic(tree, data, len, false);
}

static cbtree_result_t cbtree_insert_generic(critbit_tree_t *tree, const uint8_t *data, size_t len, bool is_str)
{
	uint32_t new_byte;
	uint8_t *ext_node, new_bitmask, val;
	int      dir;

	if (!tree->root) {
		void *ptr;

		if (!(ptr = cbtree_allocate(len)))
			return CBTREE_ENOMEM;

		if (is_str)
			memcpy(ptr, data, len + 1);
		else
			memcpy(ptr, data, len);
		tree->root = ptr;

		return CBTREE_OK;
	}

	// TODO: sacar el for y el if a una funcion auxiliar
	ext_node = cbtree_find_nearest_generic(tree, data, len, is_str, &new_byte);
	for (; new_byte < len; new_byte++) {
		if (data[new_byte] != ext_node[new_byte]) {
			new_bitmask = data[new_byte] ^ ext_node[new_byte];
			goto different_byte_found;
		}
	}

	if (is_str && ext_node[new_byte] != '\0') {
		new_bitmask = ext_node[new_byte];
		goto different_byte_found;
	}

	return CBTREE_FAIL;

different_byte_found:
	while (new_bitmask & (new_bitmask - 1))
		new_bitmask &= new_bitmask - 1;

	new_bitmask ^= UINT8_MAX;
	val = ext_node[new_byte];
	dir = GENERATE_DIR(new_bitmask, val);
}

static void *cbtree_allocate(size_t size)
{
	void *ptr;

#ifdef USE_POSIX_MEMALIGN
	if (posix_memalign(&ptr, sizeof(void *), size))
		return NULL;
#else
	ptr = malloc(size);
#endif

	return ptr;
}
