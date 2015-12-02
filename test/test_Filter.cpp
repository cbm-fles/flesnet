// Copyright 2015 Jan de Cuveland <cmail@cuveland.de>
#define BOOST_TEST_MODULE test_Filter
#include <boost/test/unit_test.hpp>

#include "Filter.hpp"
#include "Source.hpp"
#include <iostream>
#include <type_traits>
#include <cstdint>
#include <limits>

// example source: integer counter
template <typename T> class Counter : public fles::Source<T>
{
public:
    Counter(T arg_limit = std::numeric_limits<T>::max()) : limit(arg_limit)
    {
        static_assert(std::is_integral<T>::value, "Integer required.");
    };

private:
    T count = 0;
    T limit;

    virtual T* do_get() override
    {
        if (this->eof_)
            return nullptr;

        T item = count;
        ++count;
        if (count >= limit) {
            this->eof_ = true;
        }
        return new T(item);
    }
};

// example sink: item dumper
template <typename T> class Dumper : public fles::Sink<T>
{
public:
    virtual void put(const T& item) override { std::cout << item << "\n"; }
};

// example filter 1: integer doubler
template <typename T> class Doubler : public fles::Filter<T>
{
    virtual std::pair<std::unique_ptr<T>, bool>
    exchange_item(const T* item) override
    {
        if (!item) {
            return std::make_pair(std::unique_ptr<T>(nullptr), false);
        }
        T i = *item * 2;
        return std::make_pair(std::unique_ptr<T>(new T(i)), false);
    }
};

// example filter 2: integer pair adder
template <typename T> class PairAdder : public fles::BufferingFilter<T>
{
private:
    void process() override
    {
        // aggregate two consecutive input items into single sum item
        while (this->input.size() >= 2) {
            T item1 = this->input.front();
            this->input.pop_front();
            T item2 = this->input.front();
            this->input.pop_front();
            T sum = item1 + item2;
            this->output.push(sum);
        }
    }
};

BOOST_AUTO_TEST_CASE(int_filter_test)
{
    Counter<int> counter(12);

    Doubler<int> doubler1;
    PairAdder<int> pair_adder;

    fles::FilteredSource<int> source1(counter, doubler1);
    fles::FilteredSource<int> source2(source1, pair_adder);

    Doubler<int> doubler2;

    Dumper<int> sink;

    fles::FilteringSink<int> sink1(sink, doubler2);

    std::size_t count = 0;
    while (auto item = source2.get()) {
        std::cout << count << ": ";
        sink1.put(*item);
        ++count;
        if (count == 10) {
            break;
        }
    }

    BOOST_CHECK_EQUAL(count, 6);
}