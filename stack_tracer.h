#ifndef STACK_TRACER_H
#define STACK_TRACER_H
/// @brief Scans context of all called functions, up to _start, and also their stacks.
/// @param stack_cnt maximum number of stacks to trace.
void print_stackframe(int stack_cnt);
#endif // STACK_TRACER_H