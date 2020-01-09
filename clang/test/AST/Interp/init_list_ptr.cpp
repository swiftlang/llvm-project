// RUN: %clang_cc1 -std=c++17 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++17 -fsyntax-only %s -verify
// expected-no-diagnostics

namespace std {
  typedef decltype(sizeof(int)) size_t;

  template <class _E>
  class initializer_list
  {
    const _E* __begin_;
    const _E* __end_;

    constexpr initializer_list(const _E* __b, const _E* __e)
      : __begin_(__b),
        __end_(__e)
    {}

  public:
    typedef _E        value_type;
    typedef const _E& reference;
    typedef const _E& const_reference;
    typedef size_t    size_type;

    typedef const _E* iterator;
    typedef const _E* const_iterator;

    constexpr initializer_list() : __begin_(nullptr), __end_(nullptr) {}

    constexpr size_t    size()  const {return __end_ - __begin_; }
    constexpr const _E* begin() const {return __begin_;}
    constexpr const _E* end()   const {return __end_;}
  };
}

namespace InitializerList {
  constexpr int sum(const int *b, const int *e) {
    return b != e ? *b + sum(b+1, e) : 0;
  }
  constexpr int sum(std::initializer_list<int> ints) {
    return sum(ints.begin(), ints.end());
  }

  using A = int[15];
  using A = int[sum({1, 2, 3, 4, 5})];

  /*
  using B = int[1];
  using B = int[*std::initializer_list<int>{1, 2, 3}.begin()];

  using C = int[3];
  using C = int[std::initializer_list<int>{1, 2, 3}.begin()[2]];
  */
}
