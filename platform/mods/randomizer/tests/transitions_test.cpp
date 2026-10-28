#include "../transitions.h"
#include <cassert>

int main()
{
    sms_randomizer::Transitions table;
    assert(table.swap_exit(0x0200) == 0x0200);
    assert(table.level_index(0xffff) == 0xffff);
    table.exits = {{0x0200, 0x0301}, {0x0301, 0x0200}, {0x0200, 0x0900}};
    table.level_indices = {{0x0301, 7}, {0x0301, 9}};
    assert(table.swap_exit(0x0200) == 0x0301);
    assert(table.swap_exit(0x0301) == 0x0200);
    assert(table.swap_exit(0x0900) == 0x0900);
    assert(table.level_index(table.swap_exit(0x0200)) == 7);
}
