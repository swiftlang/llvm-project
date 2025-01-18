class Base {};
class Sub : public Base {};

char *operator""_c(const char *c, unsigned long) { return const_cast<char *>(c); }
