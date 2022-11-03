#include <yasl/yasl.h>
#include <yasl/yasl_aux.h>
#include "yasl/interpreter/YASL_Object.h"
#include "yasl/interpreter/VM.h"

#define TUPLE_PRE "tuple"

static const char *TUPLE_NAME = "tuple";

struct YASL_Tuple {
	size_t len;
	struct YASL_Object items[];
};

void tuple_free(struct YASL_State *S, struct YASL_Tuple *tuple) {
	for (size_t i = 0; i < tuple->len; i++) {
		dec_ref(tuple->items + i);
	}
	free(tuple);
}

static struct YASL_Tuple *tuple_alloc(size_t len) {
	struct YASL_Tuple *tuple = malloc(sizeof(struct YASL_Tuple) + len * sizeof(struct YASL_Object));
	tuple->len = len;
	return tuple;
}

static void YASL_pushtuple(struct YASL_State *S, struct YASL_Tuple *tuple) {
	YASL_pushuserdata(S, tuple, TUPLE_NAME, (void (*)(struct YASL_State *, void *))tuple_free);
	YASL_loadmt(S, TUPLE_PRE);
	YASL_setmt(S);
}

static int YASL_tuple_new(struct YASL_State *S) {
	yasl_int len = YASL_peekvargscount(S);
	struct YASL_Tuple *tuple = tuple_alloc(len);
	while (len-- > 0) {
		if (!YASL_isbool(S) && !YASL_isfloat(S) && !YASL_isint(S) &&
		    !YASL_isstr(S) && !YASL_isundef(S)) {
			YASL_print_err(S, "TypeError: Tuples may only contain immutable values.");
			YASL_throw_err(S, YASL_TYPE_ERROR);
		}
		struct YASL_Object val = vm_pop((struct VM *)S);
		inc_ref(&val);
		tuple->items[len] = val;
	}

	YASL_pushtuple(S, tuple);

	return 1;
}

static struct YASL_Tuple *YASLX_checkntuple(struct YASL_State *S, const char *name, unsigned n) {
	return (struct YASL_Tuple *)YASLX_checknuserdata(S, TUPLE_NAME, name, n);
}

static int YASL_tuple___len(struct YASL_State *S) {
	struct YASL_Tuple *tuple = YASLX_checkntuple(S, "tuple.__len", 0);
	YASL_pushint(S, tuple->len);
	return 1;
}

static int YASL_tuple___get(struct YASL_State *S) {
	struct YASL_Tuple *tuple = YASLX_checkntuple(S, "tuple.__get", 0);
	yasl_int n = YASLX_checknint(S, "tuple.__get", 1);

	if (n < 0 || n >= tuple->len) {
		YASL_print_err(S, "ValueError: unable to index tuple of length %ld with index %ld.", tuple->len, n);
		YASL_throw_err(S, YASL_VALUE_ERROR);
	}

	vm_push((struct VM *)S, tuple->items[n]);
	return 1;
}

static int YASL_tuple_tostr(struct YASL_State *S) {
	struct YASL_Tuple *tuple = YASLX_checkntuple(S, "tuple.tostr", 0);
	size_t buffer_size = 8;
	size_t buffer_count = 0;
	char *buffer = malloc(buffer_size);
	strcpy(buffer, "tuple(");
	buffer_count += strlen("tuple(");
	size_t tuple_len = tuple->len;
	if (tuple_len == 0) {
		YASL_pushlit(S, "tuple()");
		return 1;
	}
	for (size_t i = 0; i < tuple_len; i++) {
		vm_push((struct VM *)S, tuple->items[i]);
		vm_stringify_top((struct VM *)S);
		const char *s = YASL_popcstr(S);
		size_t s_len = strlen(s);
		if (buffer_count + s_len + 2 >= buffer_size) {
			buffer_size = buffer_count + s_len + 2;
			buffer_size *= 2;
			buffer = realloc(buffer, buffer_size);
		}
		strcpy(buffer + buffer_count, s);
		buffer_count += s_len;
		buffer[buffer_count++] = ',';
		buffer[buffer_count++] = ' ';
	}
	buffer_count -= 2;
	buffer[buffer_count++] = ')';
	buffer[buffer_count++] = '\0';

	YASL_pushzstr(S, buffer);
	return 1;
}

static int YASL_tuple_tolist(struct YASL_State *S) {
	struct YASL_Tuple *tuple = YASLX_checkntuple(S, "tuple.tolist", 0);

	YASL_pushlist(S);
	for (size_t i = 0; i < tuple->len; i++) {
		vm_push((struct VM *)S, tuple->items[i]);
		YASL_listpush(S);
	}
	return 1;
}

static int YASL_tuple_spread(struct YASL_State *S) {
	struct YASL_Tuple *tuple = YASLX_checkntuple(S, "tuple.spread", 0);

	for (size_t i = 0; i < tuple->len; i++) {
		vm_push((struct VM *)S, tuple->items[i]);
	}
	return tuple->len;
}

static int YASL_tuple___eq(struct YASL_State *S) {
	struct YASL_Tuple *left = YASLX_checkntuple(S, "tuple.__eq", 0);
	struct YASL_Tuple *right = YASLX_checkntuple(S, "tuple.__eq", 1);

	if (left->len != right->len) {
		YASL_pushbool(S, false);
		return 1;
	}

	for (size_t i = 0; i < left->len; i++) {
		vm_push((struct VM *)S, left->items[i]);
		vm_push((struct VM *)S, right->items[i]);
		vm_EQ((struct VM *)S);
		if (!YASL_popbool(S)) {
			YASL_pushbool(S, false);
			return 1;
		}
	}
	YASL_pushbool(S, true);
	return 1;
}

static int YASL_tuple___add(struct YASL_State *S) {
	struct YASL_Tuple *left = YASLX_checkntuple(S, "tuple.__add", 0);
	struct YASL_Tuple *right = YASLX_checkntuple(S, "tuple.__add", 1);

	size_t new_len = left->len + right->len;
	struct YASL_Tuple *v = tuple_alloc(new_len);

	for (size_t i = 0; i < left->len; i++) {
		v->items[i] = left->items[i];
	}

	for (size_t i = 0; i < right->len; i++) {
		v->items[i + left->len] = right->items[i];
	}

	for (size_t i = 0; i < v->len; i++) {
		inc_ref(v->items + i);
	}

	YASL_pushtuple(S, v);
	return 1;
}

static int YASL_tuple___next(struct YASL_State *S) {
	struct YASL_Tuple *tuple = YASLX_checkntuple(S, "tuple.__next", 0);
	yasl_int curr = YASLX_checknint(S, "tuple.__next", 1);

	if (curr >= tuple->len || curr < 0) {
		YASL_pushbool(S, false);
		return 1;
	}

	YASL_pushint(S, curr + 1);
	vm_push((struct VM *)S, tuple->items[curr]);
	YASL_pushbool(S, true);
	return 3;
}

static int YASL_tuple___iter(struct YASL_State *S) {
	YASLX_checkntuple(S, "tuple.__iter", 0);
	YASL_pushcfunction(S, &YASL_tuple___next, 2);
	YASL_pushint(S, 0);
	return 2;
}

int YASL_load_dyn_lib(struct YASL_State *S) {
	YASL_pushtable(S);
	YASL_registermt(S, TUPLE_PRE);

	YASL_loadmt(S, TUPLE_PRE);
	YASL_pushlit(S, "__get");
	YASL_pushcfunction(S, YASL_tuple___get, 2);
	YASL_tableset(S);

	YASL_loadmt(S, TUPLE_PRE);
	YASL_pushlit(S, "__len");
	YASL_pushcfunction(S, YASL_tuple___len, 1);
	YASL_tableset(S);

	YASL_loadmt(S, TUPLE_PRE);
	YASL_pushlit(S, "tostr");
	YASL_pushcfunction(S, YASL_tuple_tostr, 1);
	YASL_tableset(S);

	YASL_loadmt(S, TUPLE_PRE);
	YASL_pushlit(S, "tolist");
	YASL_pushcfunction(S, YASL_tuple_tolist, 1);
	YASL_tableset(S);

	YASL_loadmt(S, TUPLE_PRE);
	YASL_pushlit(S, "__eq");
	YASL_pushcfunction(S, YASL_tuple___eq, 2);
	YASL_tableset(S);

	YASL_loadmt(S, TUPLE_PRE);
	YASL_pushlit(S, "__add");
	YASL_pushcfunction(S, YASL_tuple___add, 2);
	YASL_tableset(S);

	YASL_loadmt(S, TUPLE_PRE);
	YASL_pushlit(S, "__iter");
	YASL_pushcfunction(S, YASL_tuple___iter, 1);
	YASL_tableset(S);

	YASL_loadmt(S, TUPLE_PRE);
	YASL_pushlit(S, "spread");
	YASL_pushcfunction(S, YASL_tuple_spread, 1);
	YASL_tableset(S);

	YASL_pushcfunction(S, YASL_tuple_new, -1);

	return 1;
}
