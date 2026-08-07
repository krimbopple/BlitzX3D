#include "stdafx.h"
#include "debugtree.h"
#include "prefs.h"

#include "../bbruntime/basic.h"

IMPLEMENT_DYNAMIC(DebugTree, CTreeCtrl)
BEGIN_MESSAGE_MAP(DebugTree, CTreeCtrl)
	ON_WM_CREATE()
END_MESSAGE_MAP()

DebugTree::DebugTree() :st_nest(0) {
}

DebugTree::~DebugTree() {
}

int DebugTree::OnCreate(LPCREATESTRUCT lpCreateStruct) {
	CTreeCtrl::OnCreate(lpCreateStruct);

	SetBkColor(prefs.rgb_bkgrnd);
	SetTextColor(prefs.rgb_default);
	SetFont(&prefs.debugFont);

	return 0;
}

static std::string typeTag(Type* t) {
	if(t->intType()) return "";
	if(t->floatType()) return "#";
	if(t->stringType()) return "$";
	if(StructType* s = t->structType()) return "." + s->ident;
	if(VectorType* v = t->vectorType()) {
		std::string s = typeTag(v->elementType) + "[";
		for(int k = 0; k < v->sizes.size(); ++k) {
			if(k) s += ",";
			s += itoa(v->sizes[k] - 1);
		}
		return s + "]";
	}
	return "";
}

VOID DebugTree::sortItemAndChildren(HTREEITEM item) {
	if(item != NULL) {
		if(item == TVI_ROOT || this->ItemHasChildren(item)) {
			HTREEITEM child = this->GetChildItem(item);

			while(child != NULL) {
				sortItemAndChildren(child);
				child = this->GetNextItem(child, TVGN_NEXT);
			}

			this->SortChildren(item);
		}
	}
}

// any further will stall the debugger
static const int MAX_EXPANDED_ELEMENTS = 500;
HTREEITEM DebugTree::insertVar(void* var, Decl* d, const std::string& name, HTREEITEM it, HTREEITEM parent) {

	std::string s = name;

	ConstType* ct = d->type->constType();
	StructType* st = d->type->structType();
	VectorType* vt = d->type->vectorType();

	void* vecData = 0;
	if (ct) {
		Type* t = ct->valueType;
		s += typeTag(t);
		if(t->intType()) {
			s += "=" + itoa(ct->intValue);
		}
		else if(t->floatType()) {
			s += "=" + ftoa(ct->floatValue);
		}
		else if(t->stringType()) {
			s += "=\"" + ct->stringValue + '\"';
		}
	}
	else if(var) {
		Type* t = d->type;
		s += typeTag(t);
		if(t->intType()) {
			s += "=" + itoa(*(int*)var);
		}
		else if(t->floatType()) {
			s += "=" + ftoa(*(float*)var);
		}
		else if(t->stringType()) {
			BBStr* str = *(BBStr**)var;
			if(str) s += "=\"" + *str + '\"';
			else s += "=\"\"";
		}
		else if(st) {
			var = *(void**)var;
			if (var) var = *(void**)var;
			if (!var) s += " (Null)";
		}
		else if (vt) {
			vecData = *(void**)var;
			if (!vecData) s += " (Null)";
		}
	}

	if(it) {
		if(GetItemText(it) != s.c_str()) {
			SetItemText(it, s.c_str());
		}
	}
	else {
		it = InsertItem(s.c_str(), parent);
	}

	++st_nest;
	if(st) {
		if(var) {
			if(st_nest < 4) {
				HTREEITEM st_it = GetChildItem(it);
				for(int k = 0; k < st->fields->size(); ++k) {
					Decl* st_d = st->fields->decls[k];
					void* st_var = (char*)var + st_d->offset;

					char name[256];
					st_d->getName(name);

					st_it = insertVar(st_var, st_d, name, st_it, it);
				}
			}
		}
		else {
			while (HTREEITEM t = GetChildItem(it)) {
				DeleteItem(t);
			}
		}
	}
	else if (vt) {
		if (vecData && st_nest < 4) {
			int total = 1;
			for (int k = 0; k < (int)vt->sizes.size(); ++k) total *= vt->sizes[k];

			int shown = total < MAX_EXPANDED_ELEMENTS ? total : MAX_EXPANDED_ELEMENTS;

			std::vector<int> strides(vt->sizes.size());
			int stride = 1;
			for (int dim = 0; dim < (int)vt->sizes.size(); ++dim) {
				strides[dim] = stride;
				stride *= vt->sizes[dim];
			}

			Decl elemDecl("", vt->elementType, DECL_LOCAL);

			HTREEITEM vt_it = GetChildItem(it);
			for (int idx = 0; idx < shown; ++idx) {
				void* elem_var = (char*)vecData + idx * 4;

				std::string idxName = "(";
				for (int dim = (int)vt->sizes.size() - 1; dim >= 0; --dim) {
					int coord = (idx / strides[dim]) % vt->sizes[dim];
					if (dim != (int)vt->sizes.size() - 1) idxName += ",";
					idxName += itoa(coord);
				}
				idxName += ")";

				vt_it = insertVar(elem_var, &elemDecl, idxName, vt_it, it);
			}

			if (total > shown) {
				std::string more = "... (" + itoa(total - shown) + " more)";
				if (vt_it) {
					if (GetItemText(vt_it) != more.c_str()) SetItemText(vt_it, more.c_str());
					vt_it = GetNextSiblingItem(vt_it);
				}
				else {
					InsertItem(more.c_str(), it);
				}
			}

			while (vt_it) {
				HTREEITEM next = GetNextSiblingItem(vt_it);
				DeleteItem(vt_it);
				vt_it = next;
			}
		}
		else {
			while (HTREEITEM t = GetChildItem(it)) {
				DeleteItem(t);
			}
		}
	}
	--st_nest;

	return it ? GetNextSiblingItem(it) : 0;
}

/******************************* CONSTS ***********************************/

ConstsTree::ConstsTree() {
}

void ConstsTree::reset(Environ* env) {

	HTREEITEM it = GetChildItem(TVI_ROOT);

	for(int k = 0; k < env->decls->size(); ++k) {

		Decl* d = env->decls->decls[k];
		if(!(d->kind & (DECL_GLOBAL))) continue;
		if(d->type->constType()) {

			char name[256];
			d->getName(name);

			it = insertVar(0, d, name, it, TVI_ROOT);
		}
	}

	sortItemAndChildren(TVI_ROOT);
}

/******************************* GLOBALS **********************************/

GlobalsTree::GlobalsTree() :module(0), envron(0) {
}

void GlobalsTree::reset(Module* mod, Environ* env) {
	module = mod;
	envron = env;
}

void GlobalsTree::refresh() {
	if(!module || !envron) return;

	HTREEITEM it = GetChildItem(TVI_ROOT);

	for(int k = 0; k < envron->decls->size(); ++k) {
		Decl* d = envron->decls->decls[k];
		if(!(d->kind & (DECL_GLOBAL))) continue;
		if(!d->type->constType()) {

			char name[256];
			d->getName(name);

			void* var = 0;
			module->findSymbol(("_v" + std::string(name)).c_str(), (int*)&var);
			it = insertVar(var, d, name, it, TVI_ROOT);
		}
	}

	sortItemAndChildren(TVI_ROOT);
}

/******************************** LOCALS **********************************/

LocalsTree::LocalsTree() :envron(0) {
}

void LocalsTree::reset(Environ* env) {
	envron = env;
}

void LocalsTree::refresh() {
	if(!envron || !frames.size()) return;

	HTREEITEM item = GetChildItem(TVI_ROOT);

	int n = 0;
	for(n = 0; n < frames.size(); ++n) {
		if(!item || item != frames[n].item) break;
		item = GetNextSiblingItem(item);
	}

	while(item) {
		HTREEITEM next = GetNextSiblingItem(item);
		DeleteItem(item);
		item = next;
	}

	for(; n < frames.size(); ++n) {
		item = frames[n].item = InsertItem(frames[n].func, TVI_ROOT, TVI_LAST);
		if(n < frames.size() - 1) refreshFrame(frames[n]);
	}

	refreshFrame(frames.back());
}

void LocalsTree::refreshFrame(const Frame& f) {

	HTREEITEM it = GetChildItem(f.item);

	for(int n = 0; n < f.env->decls->size(); ++n) {
		Decl* d = f.env->decls->decls[n];
		if(!(d->kind & (DECL_LOCAL | DECL_PARAM))) continue;

		char name[256];
		d->getName(name);

		if(!isalpha(name[0])) continue;
		it = insertVar((char*)f.frame + d->offset, d, name, it, f.item);
	}

	sortItemAndChildren(TVI_ROOT);
}

void LocalsTree::pushFrame(void* f, void* e, const char* func) {
	frames.push_back(Frame(f, (Environ*)e, func));
}

void LocalsTree::popFrame() {
	frames.pop_back();
}