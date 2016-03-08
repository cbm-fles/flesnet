// Copyright 2015 Jan de Cuveland <cmail@cuveland.de>
/// \file
/// \brief Defines the fles::Filter abstract base class and corresponding Source
/// and Sink stream filters.
#pragma once

#include "Source.hpp"
#include "Sink.hpp"
#include <memory>
#include <utility>
#include <deque>
#include <queue>

namespace fles
{

template <class T> class Filter
{
public:
    using filter_output_t = std::pair<std::unique_ptr<T>, bool>;

    /// Exchange an item with the filter.
    virtual filter_output_t exchange_item(const T* item = nullptr) = 0;

    virtual ~Filter(){};
};

template <class T, class Derived = T> class BufferingFilter : public Filter<T>
{
public:
    virtual std::pair<std::unique_ptr<T>, bool>
    exchange_item(const T* item) override
    {
        if (item) {
            input.push_back(*item);
        }

        if (output.empty()) {
            process();
        }

        if (output.empty()) {
            return std::make_pair(std::unique_ptr<T>(nullptr), false);
        } else {
            Derived i = output.front();
            output.pop();
            bool more = !output.empty();
            return std::make_pair(std::unique_ptr<T>(new Derived(i)), more);
        }
    }

protected:
    std::deque<Derived> input;
    std::queue<Derived> output;

    virtual void process() = 0;
};

template <class T, class Derived = T> class FilteredSource : public Source<T>
{
public:
    using source_t = Source<T>;
    using filter_t = Filter<T>;

    /// Construct FilteredSource using a given source and filter
    FilteredSource(source_t& arg_source, filter_t& arg_filter)
        : source(arg_source), filter(arg_filter)
    {
    }

private:
    source_t& source;
    filter_t& filter;
    bool more = false;

    using filter_output_t = typename Filter<T>::filter_output_t;

    virtual T* do_get() override
    {
        if (this->eof_)
            return nullptr;

        filter_output_t filter_output;
        if (more) {
            filter_output = filter.exchange_item();
        } else {
            do {
                auto item = source.get();
                if (!item) {
                    this->eof_ = true;
                    return nullptr;
                }
                filter_output = filter.exchange_item(item.get());
            } while (!filter_output.first);
        }
        more = filter_output.second;
        return new Derived(*filter_output.first);
    }
};

template <class T> class FilteringSink : public Sink<T>
{
public:
    using sink_t = Sink<T>;
    using filter_t = Filter<T>;

    /// Construct FilteringSink using a given sink and filter
    FilteringSink(sink_t& arg_sink, filter_t& arg_filter)
        : sink(arg_sink), filter(arg_filter)
    {
    }

    virtual void put(const T& item) override
    {
        typename Filter<T>::filter_output_t filter_output;
        filter_output = filter.exchange_item(&item);
        if (filter_output.first) {
            sink.put(*filter_output.first);
        }
        while (filter_output.second) {
            filter_output = filter.exchange_item();
            if (filter_output.first) {
                sink.put(*filter_output.first);
            }
        }
    }

private:
    sink_t& sink;
    filter_t& filter;
};

class Microslice;
class StorableMicroslice;
using MicrosliceFilter = Filter<Microslice>;
using FilteredMicrosliceSource = FilteredSource<Microslice, StorableMicroslice>;
using FilteringMicrosliceSink = FilteringSink<Microslice>;

} // namespace fles {
