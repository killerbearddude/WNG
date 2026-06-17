// Verifies that WNG test executables run with assert() enabled in every
// configured test build. Many existing tests use assert() for both fixture setup
// and invariant checks, so disabling assertions changes test semantics.

#include <cassert>

int main()
{
    bool assertion_evaluated = false;
    assert((assertion_evaluated = true));

    return assertion_evaluated ? 0 : 1;
}
