#include "std.h"
#include "bbbank.h"
#include "bbstream.h"
#include "../MultiLang/MultiLang.h"

struct bbBank {
	char* data;
	int size, capacity;

	bbBank(int sz) :size(sz) {
		capacity = (size + 15) & ~15;
		data = new char[capacity]();
	}
	virtual ~bbBank() {
		delete[] data;
	}
	void resize(int n) {
		if(n > size) {
			if(n > capacity) {
				capacity = capacity * 3 / 2;
				if(n > capacity) capacity = n;
				capacity = (capacity + 15) & ~15;
				char* p = new char[capacity];
				memcpy(p, data, size);
				delete[] data;
				data = p;
			}
			else memset(data + size, 0, n - size);
		}
		size = n;
	}
};

static std::unordered_set<bbBank*> bank_set;

static inline void debugBank(bbBank* b, const char* function) {
	if (!bank_set.count(b)) {
		ErrorLog(function, MultiLang::bank_not_exist);
	}
}

static inline void debugBank(bbBank* b, const char* function, int offset) {
	debugBank(b, function);
	if (offset >= b->size) { 
		ErrorLog(function, MultiLang::offset_out_of_range);
	}
}

int bbVerifyBank(bbBank* b) {
	return (bool)bank_set.count(b);
}

bbBank* bbCreateBank(int size) {
	bbBank* b = new bbBank(size);
	bank_set.insert(b);
	return b;
}

void bbFreeBank(bbBank* b) {
	if(bank_set.erase(b)) delete b;
}

int bbBankSize(bbBank* b) {
	debugBank(b, "BankSize");
	return b->size;
}

void  bbResizeBank(bbBank* b, int size) {
	debugBank(b, "ResizeBank");
	b->resize(size);
}

void  bbCopyBank(bbBank* src, int src_p, bbBank* dest, int dest_p, int count) {
	debugBank(src, "CopyBank", src_p + count - 1);
	debugBank(dest, "CopyBank", dest_p + count - 1);
	memmove(dest->data + dest_p, src->data + src_p, count);
}

int  bbPeekByte(bbBank* b, int offset) {
	debugBank(b, "PeekByte", offset);
	return *(unsigned char*)(b->data + offset);
}

int  bbPeekShort(bbBank* b, int offset) {
	debugBank(b, "PeekShort", offset + 1);
	return *(unsigned short*)(b->data + offset);
}

int  bbPeekInt(bbBank* b, int offset) {
	debugBank(b, "PeekInt", offset + 3);
	return *(int*)(b->data + offset);
}

float  bbPeekFloat(bbBank* b, int offset) {
	debugBank(b, "PeekFloat", offset + 3);
	return *(float*)(b->data + offset);
}

BBStr* bbPeekString(bbBank* b, int offset) {
	debugBank(b, "PeekString", offset);
	int length = *(int*)(b->data + offset);
	return new BBStr(b->data + offset + 4, length);
}

void  bbPokeByte(bbBank* b, int offset, int value) {
	debugBank(b, "PokeByte", offset);
	*(char*)(b->data + offset) = value;
}

void  bbPokeShort(bbBank* b, int offset, int value) {
	debugBank(b, "PokeShort", offset);
	*(unsigned short*)(b->data + offset) = value;
}

void  bbPokeInt(bbBank* b, int offset, int value) {
	debugBank(b, "PokeInt", offset);
	*(int*)(b->data + offset) = value;
}

void  bbPokeFloat(bbBank* b, int offset, float value) {
	debugBank(b, "PokeFloat", offset);
	*(float*)(b->data + offset) = value;
}

int bbPokeString(bbBank* b, int offset, BBStr* str) {
	debugBank(b, "PokeString", offset);
	int length = str->length();
	*(int*)(b->data + offset) = length;
	memcpy_s(b->data + offset + 4, length, str->data(), length);
	delete str;
	return offset + 4 + length;
}

int bbBankStringSize(BBStr* str) {
	int length = str->length() + 4;
	delete str;
	return length;
}

int   bbReadBytes(bbBank* b, bbStream* s, int offset, int count) {
	debugBank(b, "ReadBytes", offset + count - 1);
	debugStream(s, "ReadBytes");
	return s->read(b->data + offset, count);
}

int   bbWriteBytes(bbBank* b, bbStream* s, int offset, int count) {
	debugBank(b, "WriteBytes", offset + count - 1);
	debugStream(s, "WriteBytes");
	return s->write(b->data + offset, count);
}

int  bbCallDLL(BBStr* dll, BBStr* fun, bbBank* in, bbBank* out) {
	if(in) debugBank(in, "CallDLL");
	if(out) debugBank(out, "CallDLL");
	int t = gx_runtime->callDll(*dll, *fun,
		in ? in->data : 0, in ? in->size : 0,
		out ? out->data : 0, out ? out->size : 0);
	delete dll; delete fun;
	return t;
}

bool bank_create() {
	return true;
}

bool bank_destroy() {
	while(bank_set.size()) bbFreeBank(*bank_set.begin());
	return true;
}

void bank_link(void(*rtSym)(const char*, void*)) {
	rtSym("%VerifyBank%bank", bbVerifyBank);
	rtSym("%CreateBank%size=0", bbCreateBank);
	rtSym("FreeBank%bank", bbFreeBank);
	rtSym("%BankSize%bank", bbBankSize);
	rtSym("ResizeBank%bank%size", bbResizeBank);
	rtSym("CopyBank%src_bank%src_offset%dest_bank%dest_offset%count", bbCopyBank);
	rtSym("%PeekByte%bank%offset", bbPeekByte);
	rtSym("%PeekShort%bank%offset", bbPeekShort);
	rtSym("%PeekInt%bank%offset", bbPeekInt);
	rtSym("#PeekFloat%bank%offset", bbPeekFloat);
	rtSym("$PeekString%bank%offset", bbPeekString);
	rtSym("PokeByte%bank%offset%value", bbPokeByte);
	rtSym("PokeShort%bank%offset%value", bbPokeShort);
	rtSym("PokeInt%bank%offset%value", bbPokeInt);
	rtSym("PokeFloat%bank%offset#value", bbPokeFloat);
	rtSym("%PokeString%bank%offset$value", bbPokeString);
	rtSym("%BankStringSize$str", bbBankStringSize);
	rtSym("%ReadBytes%bank%file%offset%count", bbReadBytes);
	rtSym("%WriteBytes%bank%file%offset%count", bbWriteBytes);
	rtSym("%CallDLL$dll_name$func_name%in_bank=0%out_bank=0", bbCallDLL);
}