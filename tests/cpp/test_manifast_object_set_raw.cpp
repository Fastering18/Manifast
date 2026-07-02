#include <iostream>
#include "manifast/Runtime.h"
#include <cassert>

int main() {
    Any* obj_any = manifast_create_object();
    ManifastObject* obj = (ManifastObject*)obj_any->ptr;

    Any val;
    val.type = ANY_NUMBER;
    val.number = 42.0;

    manifast_object_set_raw(obj, "test_key", &val);

    Any* result = manifast_object_get_raw(obj, "test_key");
    assert(result->type == ANY_NUMBER);
    assert(result->number == 42.0);

    // Update value test
    Any val_update;
    val_update.type = ANY_NUMBER;
    val_update.number = 99.0;
    manifast_object_set_raw(obj, "test_key", &val_update);

    Any* result_update = manifast_object_get_raw(obj, "test_key");
    assert(result_update->type == ANY_NUMBER);
    assert(result_update->number == 99.0);

    std::cout << "test_manifast_object_set_raw passed" << std::endl;
    return 0;
}
