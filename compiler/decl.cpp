#include "std.h"
#include "decl.h"
#include "type.h"
#include <algorithm>

Decl::~Decl() {
}

DeclSeq::DeclSeq() {
}

void Decl::getName(char* buff) {
    int sz = name.size();
    memcpy(buff, name.data(), sz);
    buff[sz] = 0;
}

DeclSeq::~DeclSeq() {
    for (; decls.size(); decls.pop_back()) delete decls.back();
}

Decl* DeclSeq::findDecl(const std::string& s) {
    auto it = declMap.find(s);
    return (it != declMap.end()) ? it->second : nullptr;
}

Decl* DeclSeq::findDecl(const std::string& s, int params) {
    Decl* decl = findDecl(s);
    if (OverrideFunctionMap.contains(s)) {
        if (decl && decl->type->funcType()->params->size() == params) return decl;
        std::string&& d = "";
        std::vector<OverrideFunction>::iterator it;
        for (it = OverrideFunctionMap[s].begin(); it != OverrideFunctionMap[s].end(); ++it)
        {
            // find the matchest function
            OverrideFunction& func = *it;
            if ((func.requiredParameters + func.optionalParameters) == params) {
                return this->findDecl(func.name);
            }
            if (func.requiredParameters == params) {
                return this->findDecl(func.name);
            }
            if ((func.requiredParameters < params) && ((func.requiredParameters + func.optionalParameters) >= params)) {
                d = func.name;
            }
        }
        if (d != "") return this->findDecl(d);
    }
    return decl;
}

Decl* DeclSeq::insertDecl(const std::string& s, Type* t, int kind, ConstType* d) {
    if (kind & DECL_FUNC) {
        FuncType* func = t->funcType();
        if (Decl* d1 = findDecl(s)) { // found duplicate!
            if (d1->type->funcType()->params->size() == func->params->size()) return 0;
            const std::string uniqueFunctionName = "blitz_"s + s + to_string(OverrideFunctionMap[s].size());
            const int requiredParameters = std::count_if(func->params->decls.begin(), func->params->decls.end(), [](Decl* d) {
                return d->defType == 0;
                });
            if (std::count_if(OverrideFunctionMap[s].begin(), OverrideFunctionMap[s].end(), [func](OverrideFunction& function) {
                return (function.requiredParameters + function.optionalParameters) == func->params->size();
                })) {
                return 0;
            }
            Decl* decl = new Decl(uniqueFunctionName, t, kind, d);
            OverrideFunctionMap[s].push_back(OverrideFunction(uniqueFunctionName, requiredParameters, func->params->size() - requiredParameters));
            declMap[uniqueFunctionName] = decl;
            decls.push_back(decl);
            return decls.back();
        }
        Decl* decl = new Decl(s, t, kind, d);
        declMap[s] = decl;
        decls.push_back(decl);
        return decls.back();
    }
    if (findDecl(s)) return 0;
    Decl* decl = new Decl(s, t, kind, d);
    declMap[s] = decl;
    decls.push_back(decl);
    return decls.back();
}