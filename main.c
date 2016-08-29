#include <stdio.h>
#include <stdlib.h>

#include <readline/readline.h>
#include "mpc/mpc.h"
// #include "chapter7.c"
//#include <editline/history.h>


#define LASSERT(args, cond, err) \
	if (!(cond)) { lval_del(args); return lval_err(err); }

struct lval;
struct lenv;

typedef struct lval lval;
typedef struct lenv lenv;

typedef lval*(*lbuiltin)(lenv*, lval*);

struct lval {
	int type;
	long num;
	char *sym;
	char *err;
	int count;
	lbuiltin fun;
	lval** cell;
};

struct lenv {
	int count;
	char** syms;
	lval** vals;
};


enum { LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_SEXPR, LVAL_QEXPR, LVAL_FUN };
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

void lval_print(lval* v);
lval* lval_take(lval* v, int i);
lval* lval_eval(lval* v);
lval* lval_pop(lval* v, int i);
lval* lval_join(lval* x, lval* y);
lval* builtin_join(lval* a);
lval* builtin(lval* a, char* func);


lenv* lenv_new(void) {
	lenv* e = malloc(sizeof(lenv));
	e->count = 0;
	e->syms = NULL;
	e->vals = NULL;
	return e;
}

void lenv_del(lenv* e) {
	for (int i = 0; i < e->count; i++ ) {
		free(e->syms[i]);
		lval_del(e->vals[i]);
	}
	free(e->syms);
	free(e->vals);
	free(e);
}

/* pointer to a new fun value */
lval* lval_fun(lbuiltin func) {
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_FUN;
	v->fun = func;
	return v;
}

/* pointer to a new Number value */
lval* lval_num(long x) {
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_NUM;
	v->num = x;
	return v;
}

/* pointer to new Error value */
lval* lval_err(char* m) {
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_ERR;
	v->err = malloc(strlen(m)+1);
	strcpy(v->err, m);
	return v;
}

lval* lval_sym(char* s) {
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_SYM;
	v->sym = malloc(strlen(s)+1);
	strcpy(v->sym, s);
	return v;
}

lval* lval_sexpr(void) {
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_SEXPR;
	v->count = 0;
	v->cell = NULL;
	return v;
}

lval* lval_qexpr(void) {
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_QEXPR;
	v->count = 0;
	v->cell = NULL;
	return v;
}

int lv_length(lval* a) {
	return a->cell[0]->count;
}

/* destructor */
void lval_del(lval* v) {
	switch(v->type) {
		case LVAL_FUN: break;
		case LVAL_NUM: break;

		case LVAL_ERR: free(v->err); break;
		case LVAL_SYM: free(v->sym); break;

		case LVAL_QEXPR:
		case LVAL_SEXPR:
		for (int i = 0; i < v->count; i++) {
			lval_del(v->cell[i]);
		}
		free(v->cell);
		break;
	}
	free(v);
}

lval* lval_read_num(mpc_ast_t* t) {
	errno = 0;
	long x = strtol(t->contents, NULL, 10);
	return errno != ERANGE ?
		lval_num(x) : lval_err("invalid number");
}

lval* lval_add(lval* v, lval* x) {
	v->count++;
	v->cell = realloc(v->cell, sizeof(lval*) * v->count);
	v->cell[v->count-1] = x;
	return v;
}

void lval_expr_print(lval* v, char open, char close) {
  putchar(open);
  for (int i = 0; i < v->count; i++) {
    
    /* Print Value contained within */
    lval_print(v->cell[i]);
    
    /* Don't print trailing space if last element */
    if (i != (v->count-1)) {
      putchar(' ');
    }
  }
  putchar(close);
}

void lval_print(lval* v) {
	switch(v->type) {
		case LVAL_FUN: printf("<function>"); break;
		case LVAL_NUM: printf("%li", v->num); break;
		case LVAL_SYM: printf("%s", v->sym); break;
		case LVAL_ERR: printf("Error: %s", v->err); break;
		case LVAL_SEXPR: lval_expr_print(v, '(', ')'); break;
		case LVAL_QEXPR: lval_expr_print(v, '{', '}'); break;
	}
}

void lval_println(lval* v) { lval_print(v); putchar('\n'); }


lval* lval_read(mpc_ast_t* t) {
	
	// if symbol or number return convertion to that type
	if (strstr(t->tag, "number")) { return lval_read_num(t); }
	if (strstr(t->tag, "symbol")) { return lval_sym(t->contents); }
	
	// if root (>) or sexpr or qexpr then create empty list 
	lval* x = NULL;
	if (strcmp(t->tag, ">") == 0) { x = lval_sexpr(); }
	if (strcmp(t->tag, "sexpr")) { x = lval_sexpr(); }
	if (strstr(t->tag, "qexpr")) {x = lval_qexpr(); }
	
	// fill this list with any valid expression 
	for (int i = 0; i < t->children_num; i++ ) {
		if (strcmp(t->children[i]->contents, "(") == 0) {continue; }
		if (strcmp(t->children[i]->contents, ")") == 0) {continue; }
		if (strcmp(t->children[i]->contents, "}") == 0) {continue; }
		if (strcmp(t->children[i]->contents, "{") == 0) {continue; }
		if (strcmp(t->children[i]->tag, "regex") == 0) {continue; }
		x = lval_add(x, lval_read(t->children[i]));
	}
	return x;
}

lval* builtin_op(lval* a, char* op) {
	for (int i = 0; i < a->count; i++){
		if (a->cell[i]->type != LVAL_NUM) {
			lval_del(a);
			return lval_err("Cannot operate on non-number");
		}
	}
	
	lval* x = lval_pop(a, 0);
	
	// if first -
	if ((strcmp(op, "-") == 0) && a->count == 0) {
		x->num = -x->num;
	}
	
	while (a->count > 0) {
		lval* y = lval_pop(a, 0);
		
		if (strcmp(op, "+") == 0) {x->num += y->num; }
		if (strcmp(op, "-") == 0) {x->num -= y->num; }
		if (strcmp(op, "*") == 0) {x->num *= y->num; }
		if (strcmp(op, "/") == 0) {
			if (y->num == 0) {
				lval_del(x); lval_del(y);
				return lval_err("Division by zero"); break;
			}
			x->num /= y->num;
		}
		lval_del(y);
	}
	lval_del(a);
	return x;
}

lval* builtin_head(lval* a) {
	// check error cond
	LASSERT(a, a->count == 1,
	    "Function 'head' passed too many arguments!");
	LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
		"Function 'head' passed incorrect types!");
	LASSERT(a, a->cell[0]->count != 0, "Function 'head' passed {}!");
	lval* v = lval_take(a, 0);

	// delete all elements that are not head and return
	while (v->count > 1) { lval_del(lval_pop(v, 1)); }
	return v;
}

lval* builtin_tail(lval* a) {
	LASSERT(a, a->count == 1,
		"Function 'tail' passed too many arguments");

	LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
		"Function 'tail' passed incorrect types!");

	LASSERT(a, a->cell[0]->count != 0,
		"Function 'tail' passed {}");
	// get first elem and free it. return without head
	lval* v = lval_take(a, 0);
	lval_del(lval_pop(v, 0));
	return v;
}

lval* builtin_init(lval* a) {
	LASSERT(a, a->count == 1,
		"Function 'init' passed to many arguments");
	LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
		"Function 'init' passed incorrect type");
	LASSERT(a, a->cell[0]->count != 0,
		"Function 'init' passed {}");

	lval_del(lval_pop(a->cell[0], lv_length(a)-1));
	return a->cell[0];
}

lval* lval_copy(lval* v) {
	lval* x = malloc(sizeof(lval));
	x->type = v->type;
	switch(v->type) {
		case LVAL_FUN: x->fun = v->fun; break;
		case LVAL_NUM: x->num = v->num; break;
		
		case LVAL_ERR:
		  x->err = malloc(strlen(v->err) + 1);
		  strcpy(x->err, v->err); break;
		
		case LVAL_SYM:
		  x->sym = malloc(strlen(v->sym) + 1);
		  strcpy(x->sym, v->sym); break;
		  
		case LVAL_QEXPR:
		case LVAL_SEXPR:
		  x->count = v->count;
		  x->cell = malloc(sizeof(*lval) * x->count);
		  for (int i = 0; i < x->count; i++ ) {
			  x->cell[i] = lval_copy(v->cell[i]);
		  }
		  break;
	}
	return x;
}

lval* lenv_get(lenv* e, lval* k) {
	
	// for all items in env
	for (int i = 0; i < e->count; i++) {
		if (strcmp(e->syms[i], k->sym) == 0) {
			return lval_copy(e->valls[i]);
		}
	}
	// if not found, return error
	return lval_err("unbound symbol");
}

void lenv_put(lenv* e, lval* k, lval* v) {
	for (int i = 0; i < e->count, i++) {
		if (strcmp(e->syms[i], k->sym) == 0) {
			lval_del(e->vals[i]);
			e->vals[i] = lval_copy(v);
			return;
		}
	}
	
	e->count++;
	e->vals = realloc(e->vals, sizeof(lval*) * e->count);
	e->syms = realloc(e->syms, sizeof(char*) * e->count);
	
	e->vals[e->count-1] = lval_copy(v);
	e->syms[e->count-1] = malloc(strlen(k->sym)+1);
	strcpy(e->syms[e->count-1], k->sym);
}

lval* lval_eval(lenv* e, lval* v) {
	if (v->type == LVAL_SYM) {
		lval* x = lenv_get(e, v);
		lval_del(v);
		return x;
	}
	if (v->type == LVAL_SEXPR) {
		return lval_eval_sexpr(e, v);
	}
	return v;
}

lval* lval_eval_sexpr(lenv* e, lval* v) {
	for (int i = 0; i < v->count; i++) {
		v->cell[i] = lval_eval(e, v->cell[i]);
	}
	for (int i = 0; i < v->count; i++) {
		if (v->cell[i]->type == LVAL_ERR) { return lval_take(v, i);}
	}
	
	if (v->count == 0) { return v;}
	if (v->count == 1) {return lval_take(v, 0);}
	
	lval* f = lval_pop(v, 0);
	if (f->type != LVAL_FUN) {
		lval_del(v); lval_del(f);
		return lval_err("first element is not a function")
	}
	lval* res = f->fun(e, v);
	lval_del(f);
	return res;
}

// TODO make cons
// lval* builtin_cons(lval* a, lval* b) {
//     LASSERT(a, a->count == 1,
// 		"Function 'cons' passed to many arguments");
// 	LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
// 		"Function 'cons' passed incorrect type");
// 	LASSERT(a, a->cell[0]->count != 0,
// 		"Function 'cons' passed {}");
//
//     lval* v = lval_qexpr();
//
//     // v->cell add fst *b, then append a
//     return v;
// }

lval* builtin_list(lval* a) {
	a->type = LVAL_QEXPR;
	return a;
}

lval* builtin_eval(lval* a) {
	LASSERT(a, a->count == 1,
	"Function 'eval' passed to many arguments");
	LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
	"Function 'eval' passed incorrect type");

	lval* x = lval_take(a, 0);
	x->type = LVAL_SEXPR;
	return lval_eval(x);
}

lval* builtin_join(lval* a) {
	for (int i = 0; i < a->count; i++) {
		LASSERT(a, a->cell[i]->type == LVAL_QEXPR,
		"Function 'join' passed incorrect type.");
	}

	lval* x = lval_pop(a, 0);
	while (a->count) {
		x = lval_join(x, lval_pop(a, 0));
	}
	lval_del(a);
	return x;
}

lval* lval_join(lval* x, lval* y) {
	while (y->count) {
		x = lval_add(x, lval_pop(y, 0));
	}
	lval_del(y);
	return x;
}

// lval_pop will pop element at index i and shifts all elements
lval* lval_pop(lval* v, int i) {
	lval* x = v->cell[i]; 
	
	memmove(&v->cell[i], &v->cell[i+1], sizeof(lval*) * (v->count-i-1));
	v->count--;
	v->cell = realloc(v->cell, sizeof(lval*) * (v->count));
	return x;
}

lval* lval_take(lval* v, int i) {
	lval* x = lval_pop(v, i);
	lval_del(v);
	return x;
}

lval* lval_eval_sexpr(lval* v) {
	// eval children
	for (int i = 0; i < v->count; i++ ) {
		v->cell[i] = lval_eval(v->cell[i]);
	}
	
	for (int i = 0; i < v->count; i++) {
	    if (v->cell[i]->type == LVAL_ERR) { return lval_take(v, i); }
	}
	
	// empty?
	if (v->count == 0) { return v; }
	
	// single
	if (v->count == 1) { return lval_take(v, 0); }
	
	lval* f = lval_pop(v, 0);
	
	if (f->type != LVAL_SYM) {
		lval_del(f); lval_del(v);
		return lval_err("S-expr does not start with symbol");
	}
	  /* Call builtin with operator */
	lval* result = builtin(v, f->sym);
	lval_del(f);
	return result;
}

lval* lval_eval(lval* v) {
	if (v->type == LVAL_SEXPR) { return lval_eval_sexpr(v); }
	return v;
}

lval* builtin(lval* a, char* func) {
  if (strcmp("list", func) == 0) { return builtin_list(a); }
  if (strcmp("head", func) == 0) { return builtin_head(a); }
  if (strcmp("tail", func) == 0) { return builtin_tail(a); }
  if (strcmp("eval", func) == 0) { return builtin_eval(a); }
  if (strcmp("join", func) == 0) { return builtin_join(a); }
  if (strcmp("init", func) == 0) { return builtin_init(a); }
  if (strstr("+-/*", func)) { return builtin_op(a, func); }
  lval_del(a);
  return lval_err("Unknown function!");
}

int main(int argc, char** argv) {
  
  /* Create Some Parsers */
  mpc_parser_t* Number   = mpc_new("number");
  mpc_parser_t* Symbol   = mpc_new("symbol");
  mpc_parser_t* Sexpr    = mpc_new("sexpr");
  mpc_parser_t* Expr     = mpc_new("expr");
  mpc_parser_t* Qexpr    = mpc_new("qexpr");
  mpc_parser_t* Lispy    = mpc_new("lispy");
  
  /* Define them with the following Language */
  mpca_lang(MPCA_LANG_DEFAULT,
    "                                                   							\
      number   : /-?[0-9]+/ ;                           							\
      symbol   : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/ ;									\
	  sexpr    : '(' <expr>* ')' ;                      							\
	  qexpr	   : '{' <expr>* '}' ; 													\
      expr     : <number> | <symbol> | <sexpr> | <qexpr> ;  						\
      lispy    : /^/ <expr>* /$/ ;             										\
    ",
    Number, Symbol, Sexpr, Qexpr, Expr, Lispy);
  
  puts("Lispy Version 0.0.0.0.2");
  puts("Press Ctrl+c to Exit\n");
  
  while (1) {
  
    char* input = readline("lispy> ");
    add_history(input);
    
    /* Attempt to parse the user input */
    mpc_result_t r;
    if (mpc_parse("<stdin>", input, Lispy, &r)) {
		// lval result = eval(r.output);
		lval* x = lval_eval(lval_read(r.output));
		lval_println(x);
		lval_del(x);


    	mpc_ast_delete(r.output);
    } else {
        /* Otherwise print and delete the Error */
        mpc_err_print(r.error);
        mpc_err_delete(r.error);
    }
    
      free(input);
  }
  
  /* Undefine and delete our parsers */
  mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);
  
  return 0;
}
