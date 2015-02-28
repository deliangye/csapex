/// HEADER
#include <csapex/utility/assert.h>

/// SYSTEM
#include <assert.h>
#include <stdexcept>
#include <sstream>
#include <iostream>
#include <signal.h>
#include <execinfo.h>

void _apex_assert(bool assertion, const char* code, const char* file, int line)
{
    _apex_assert_hard(assertion, code, file, line);
}

void _apex_assert_hard(bool assertion, const char* code, const char* file, int line)
{
    if(!assertion) {
        std::cerr << "assertion \"" << code << "\" failed in " << file << ", line " << line << std::endl;
//        std::cerr.flush();

//        for(std::size_t i = 0; i < 20; ++i) {
//            std::cerr.flush();
//        }

//        const size_t max_depth = 100;
//        size_t stack_depth;
//        void *stack_addrs[max_depth];
//        char **stack_strings;

//        stack_depth = backtrace(stack_addrs, max_depth);
//        stack_strings = backtrace_symbols(stack_addrs, stack_depth);

//        std::cerr << "Call stack from " <<  file << " : " << line << '\n';

//        for (size_t i = 1; i < stack_depth; i++) {
//            std::cerr << "    " << stack_strings[i] << '\n';
//        }
//        free(stack_strings); // malloc()ed by backtrace_symbols

//        std::cerr << std::flush;

        std::abort();
    }
}

void _apex_assert_soft(bool assertion, const char* code, const char* file, int line)
{
    if(!assertion) {
        std::cerr << "[cs::APEX - SOFT ASSERTION FAILED] \"" << code << "\" [file " << file << ", line " << line << "]" << std::endl;
    }
}
