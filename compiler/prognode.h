#ifndef PROGNODE_H
#define PROGNODE_H

#include "node.h"
#include "codegen.h"

struct UserFunc {
	std::string ident, proc, lib;
	UserFunc(const UserFunc& t) :ident(t.ident), proc(t.proc), lib(t.lib) {}
	UserFunc(const std::string& id, const std::string& pr, const std::string& lb) :ident(id), proc(pr), lib(lb) {}
};

struct ProgNode : public Node {

	DeclSeqNode* consts;
	DeclSeqNode* structs;
	DeclSeqNode* funcs;
	DeclSeqNode* datas;
	DeclSeqNode* enums;
	StmtSeqNode* stmts;

	Environ* sem_env;

	std::string file_lab;

	ProgNode(DeclSeqNode* c, DeclSeqNode* s, DeclSeqNode* f, DeclSeqNode* d, DeclSeqNode* e, StmtSeqNode* ss) :consts(c), structs(s), funcs(f), datas(d), enums(e), stmts(ss) {}
	~ProgNode() {
		delete consts;
		delete structs;
		delete funcs;
		delete datas;
		delete stmts;
	}

	Environ* semant(Environ* e);
	void translate(Codegen* g, const std::vector<UserFunc>& userfuncs);
};

#endif