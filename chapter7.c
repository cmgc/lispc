int number_of_nodes(mpc_ast_t* t) {
  if (t->children_num == 0) { return 1; }
  if (t->children_num >= 1) {
    int total = 1;
    for (int i = 0; i < t->children_num; i++) {
      total = total + number_of_nodes(t->children[i]);
    }
    return total;
  }
}


/* next functions may not work properly */
long num_leaves(mpc_ast_t* t) {
	// also validate if contents has '(' or ')'
	if (t->children_num == 0) { 
		if (strlen(t->contents) != 0) {
			return 1; 
		}
		return 0;
	}
	long leaves = 0;
	for (int i = 0; i < t->children_num; i++ ) {
		leaves += num_leaves(t->children[i]);
	}
	return leaves;
}

long num_branches(mpc_ast_t* t) {
    if (t->children_num == 0 && strlen(t->contents) == 0) { return 1; }
	if (strlen(t->contents) != 0) {
		return 0;
	}
	long branches = 1;
	for ( int i = 0; i < t->children_num; i++ ) {
    	branches += num_branches(t->children[i]);
    }
    return branches;
}