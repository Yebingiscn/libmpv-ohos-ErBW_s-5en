__attribute__((visibility("hidden"))) int thinlto_leaf(int value);
__attribute__((visibility("default"))) int thinlto_entry(int value)
{
    return thinlto_leaf(value) + 1;
}
