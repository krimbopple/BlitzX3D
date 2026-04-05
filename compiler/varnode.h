#ifndef VARNODE_H
#define VARNODE_H

#include "varnode.h"

struct VarNode : public Node {
	DECLARE_ARENA_ALLOCATION()

	Type* sem_type;

	//get set var
	TNode* load(Codegen* g);
	virtual TNode* store(Codegen* g, TNode* n);
	virtual bool isObjParam();

	//addr of var
	virtual void semant(Environ* e) = 0;
	virtual TNode* translate(Codegen* g) = 0;
};

#include "decl.h"

struct DeclVarNode : public VarNode {
	Decl* sem_decl;
	DeclVarNode(Decl* d = 0) :sem_decl(d) { if(d) sem_type = d->type; }
	void semant(Environ* e);
	TNode* translate(Codegen* g);
	virtual TNode* store(Codegen* g, TNode* n);
	bool isObjParam();
};

struct IdentVarNode : public DeclVarNode {
	std::string ident, tag;
	IdentVarNode(const std::string& i, const std::string& t) :ident(i), tag(t) {}
	void semant(Environ* e);
};

struct ArrayVarNode : public VarNode {
	std::string ident, tag;
	ExprSeqNode* exprs;
	Decl* sem_decl;
	ArrayVarNode(const std::string& i, const std::string& t, ExprSeqNode* e) :ident(i), tag(t), exprs(e) {}
	~ArrayVarNode() { delete exprs; }
	void semant(Environ* e);
	TNode* translate(Codegen* g);
};

struct FieldVarNode : public VarNode {
	ExprNode* expr;
	std::string ident, tag;
	Decl* sem_field;
	FieldVarNode(ExprNode* e, const std::string& i, const std::string& t) :expr(e), ident(i), tag(t) {}
	~FieldVarNode() { delete expr; }
	void semant(Environ* e);
	TNode* translate(Codegen* g);
};

struct VectorVarNode : public VarNode {
	ExprNode* expr;
	ExprSeqNode* exprs;
	VectorType* vec_type;
	VectorVarNode(ExprNode* e, ExprSeqNode* es) :expr(e), exprs(es) {}
	~VectorVarNode() { delete expr; delete exprs; }
	void semant(Environ* e);
	TNode* translate(Codegen* g);
};

#endif