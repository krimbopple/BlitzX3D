#ifndef ARENA_H
#define ARENA_H

#include <cstdlib>
#include <vector>
#include <cassert>

class Arena {
public:
    explicit Arena(size_t blockSize = 1024 * 1024)
        : blockSize(blockSize), currentBlock(nullptr), currentPos(0) {
    }

    ~Arena() {
        for (void* block : blocks) {
            free(block);
        }
    }

    void* allocate(size_t size) {
        size = (size + 7) & ~7;

        if (!currentBlock || currentPos + size > blockSize) {
            void* newBlock = malloc(blockSize);
            if (!newBlock) throw std::bad_alloc();
            blocks.push_back(newBlock);
            currentBlock = static_cast<char*>(newBlock);
            currentPos = 0;
        }

        void* result = currentBlock + currentPos;
        currentPos += size;
        return result;
    }

    template<typename T, typename... Args>
    T* create(Args&&... args) {
        void* mem = allocate(sizeof(T));
        return new (mem) T(std::forward<Args>(args)...);
    }

    void reset() {
        currentBlock = nullptr;
        currentPos = 0;
    }

private:
    size_t blockSize;
    char* currentBlock;
    size_t currentPos;
    std::vector<void*> blocks;
};

extern Arena g_nodeArena;

#define DECLARE_ARENA_ALLOCATION() \
    void* operator new(size_t size) { return g_nodeArena.allocate(size); } \
    void operator delete(void*) {}

#endif