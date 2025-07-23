#ifndef __FUNCTION_H__
#define __FUNCTION_H__

#include<iostream>
#include<dlfcn.h>
#include<stdexcept>
#include<string>

namespace mxvm {

    template<typename Sig>
    class Function;
    template<typename R, typename... Args>
    class Function<R(Args...)> {
    public:
        using Fn = R(*)(Args...);
        Function(char const* lib, char const* name) {
            handle = dlopen(lib, RTLD_LAZY);
            if (!handle) throw std::runtime_error(dlerror());
                void* sym = dlsym(handle, name);
                if (!sym) {
                    dlclose(handle);
                    throw std::runtime_error(dlerror());
            }
            fn = reinterpret_cast<Fn>(sym);
        }

        Function(char const* name) {
            handle = dlopen(nullptr, RTLD_LAZY);
            if (!handle) throw std::runtime_error(dlerror());
                void* sym = dlsym(handle, name);
                if (!sym) {
                    dlclose(handle);
                    throw std::runtime_error(dlerror());
            }
            fn = reinterpret_cast<Fn>(sym);
        }

        ~Function() {
            if (handle) dlclose(handle);
        }
        R operator()(Args... args) const {
            if (!fn) throw std::runtime_error("Null function pointer");
            return fn(std::forward<Args>(args)...);
        }
    private:
        void* handle = nullptr;
        Fn fn = nullptr;
    };

    
}

#endif