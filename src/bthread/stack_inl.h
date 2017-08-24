// bthread - A M:N threading library to make applications more concurrent.
// Copyright (c) 2012 Baidu.com, Inc. All Rights Reserved

// Author: Ge,Jun (gejun@baidu.com)
// Date: Sun Sep  7 22:37:39 CST 2014

#ifndef BAIDU_BTHREAD_ALLOCATE_STACK_INL_H
#define BAIDU_BTHREAD_ALLOCATE_STACK_INL_H

DECLARE_int32(guard_page_size);
DECLARE_int32(tc_stack_small);
DECLARE_int32(tc_stack_normal);

namespace bthread {

struct MainStackClass {};

struct SmallStackClass {
    static int* stack_size_flag;
    // Older gcc does not allow static const enum, use int instead.
    static const int stacktype = (int)STACK_TYPE_SMALL;
};

struct NormalStackClass {
    static int* stack_size_flag;
    static const int stacktype = (int)STACK_TYPE_NORMAL;
};

struct LargeStackClass {
    static int* stack_size_flag;
    static const int stacktype = (int)STACK_TYPE_LARGE;
};

template <typename StackClass> struct StackFactory {
    struct Wrapper : public ContextualStack {
        explicit Wrapper(void (*entry)(intptr_t)) {
            if (allocate_stack_storage(&storage, *StackClass::stack_size_flag,
                                       FLAGS_guard_page_size) != 0) {
                storage.zeroize();
                context = NULL;
                return;
            }
            context = bthread_make_fcontext(storage.bottom, storage.stacksize, entry);
            stacktype = (StackType)StackClass::stacktype;
        }
        ~Wrapper() {
            if (context) {
                context = NULL;
                deallocate_stack_storage(&storage);
                storage.zeroize();
            }
        }
    };
    
    static ContextualStack* get_stack(void (*entry)(intptr_t)) {
        return base::get_object<Wrapper>(entry);
    }
    
    static void return_stack(ContextualStack* sc) {
        base::return_object(static_cast<Wrapper*>(sc));
    }
};

template <> struct StackFactory<MainStackClass> {
    static ContextualStack* get_stack(void (*)(intptr_t)) {
        ContextualStack* s = new (std::nothrow) ContextualStack;
        if (NULL == s) {
            return NULL;
        }
        s->context = NULL;
        s->stacktype = STACK_TYPE_MAIN;
        s->storage.zeroize();
        return s;
    }
    
    static void return_stack(ContextualStack* s) {
        delete s;
    }
};

inline ContextualStack* get_stack(StackType type, void (*entry)(intptr_t)) {
    switch (type) {
    case STACK_TYPE_PTHREAD:
        return NULL;
    case STACK_TYPE_SMALL:
        return StackFactory<SmallStackClass>::get_stack(entry);
    case STACK_TYPE_NORMAL:
        return StackFactory<NormalStackClass>::get_stack(entry);
    case STACK_TYPE_LARGE:
        return StackFactory<LargeStackClass>::get_stack(entry);
    case STACK_TYPE_MAIN:
        return StackFactory<MainStackClass>::get_stack(entry);
    }
    return NULL;
}

inline void return_stack(ContextualStack* s) {
    if (NULL == s) {
        return;
    }
    switch (s->stacktype) {
    case STACK_TYPE_PTHREAD:
        assert(false);
        return;
    case STACK_TYPE_SMALL:
        return StackFactory<SmallStackClass>::return_stack(s);
    case STACK_TYPE_NORMAL:
        return StackFactory<NormalStackClass>::return_stack(s);
    case STACK_TYPE_LARGE:
        return StackFactory<LargeStackClass>::return_stack(s);
    case STACK_TYPE_MAIN:
        return StackFactory<MainStackClass>::return_stack(s);
    }
}

inline void jump_stack(ContextualStack* from, ContextualStack* to) {
    bthread_jump_fcontext(&from->context, to->context, 0/*not skip remained*/);
}

}  // namespace bthread

namespace base {

template <> struct ObjectPoolBlockMaxItem<
    bthread::StackFactory<bthread::LargeStackClass>::Wrapper> {
    static const size_t value = 64;
};
template <> struct ObjectPoolBlockMaxItem<
    bthread::StackFactory<bthread::NormalStackClass>::Wrapper> {
    static const size_t value = 64;
};

template <> struct ObjectPoolBlockMaxItem<
    bthread::StackFactory<bthread::SmallStackClass>::Wrapper> {
    static const size_t value = 64;
};

template <> struct ObjectPoolFreeChunkMaxItem<
    bthread::StackFactory<bthread::SmallStackClass>::Wrapper> {
    inline static size_t value() {
        return (FLAGS_tc_stack_small <= 0 ? 0 : FLAGS_tc_stack_small);
    }
};

template <> struct ObjectPoolFreeChunkMaxItem<
    bthread::StackFactory<bthread::NormalStackClass>::Wrapper> {
    inline static size_t value() {
        return (FLAGS_tc_stack_normal <= 0 ? 0 : FLAGS_tc_stack_normal);
    }
};

template <> struct ObjectPoolFreeChunkMaxItem<
    bthread::StackFactory<bthread::LargeStackClass>::Wrapper> {
    inline static size_t value() { return 1UL; }
};

template <> struct ObjectPoolValidator<
    bthread::StackFactory<bthread::LargeStackClass>::Wrapper> {
    inline static bool validate(
        const bthread::StackFactory<bthread::LargeStackClass>::Wrapper* w) {
        return w->context != NULL;
    }
};

template <> struct ObjectPoolValidator<
    bthread::StackFactory<bthread::NormalStackClass>::Wrapper> {
    inline static bool validate(
        const bthread::StackFactory<bthread::NormalStackClass>::Wrapper* w) {
        return w->context != NULL;
    }
};

template <> struct ObjectPoolValidator<
    bthread::StackFactory<bthread::SmallStackClass>::Wrapper> {
    inline static bool validate(
        const bthread::StackFactory<bthread::SmallStackClass>::Wrapper* w) {
        return w->context != NULL;
    }
};
    
}  // namespace base

#endif  // BAIDU_BTHREAD_ALLOCATE_STACK_INL_H