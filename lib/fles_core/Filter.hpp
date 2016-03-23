// Copyright 2015 Jan de Cuveland <cmail@cuveland.de>
/// \file
/// \brief Defines the fles::Filter abstract base class and corresponding Source
/// and Sink stream filters.
#pragma once

#include "Sink.hpp"
#include "Source.hpp"
#include <deque>
#include <memory>
#include <queue>
#include <utility>

namespace fles
{

template <class Input, class Output = Input> class Filter
{
public:
    using filter_output_t = std::pair<std::unique_ptr<Output>, bool>;

    /// Exchange an item with the filter.
    virtual filter_output_t
    exchange_item(std::shared_ptr<const Input> item = nullptr) = 0;

    virtual ~Filter() = default;
};

template <class Input, class Output = Input>
class BufferingFilter : public Filter<Input, Output>
{
public:
    std::pair<std::unique_ptr<Output>, bool>
    exchange_item(std::shared_ptr<const Input> item) override
    {
        if (item) {
            input.push_back(item);
        }

        if (output.empty()) {
            process();
        }

        if (output.empty()) {
            return std::make_pair(std::unique_ptr<Output>(nullptr), false);
        }
        auto i = std::move(output.front());
        output.pop();
        bool more = !output.empty();
        return std::make_pair(std::move(i), more);
    }

protected:
    std::deque<std::shared_ptr<const Input>> input;
    std::queue<std::unique_ptr<Output>> output;

    virtual void process() = 0;
};

template <class Input, class Output = Input>
class FilteredSource : public Source<Output>
{
public:
    using source_t = Source<Input>;
    using filter_t = Filter<Input, Output>;

    /// Construct FilteredSource using a given source and filter
    FilteredSource(source_t& arg_source, filter_t& arg_filter)
        : source(arg_source), filter(arg_filter)
    {
    }

    bool eos() const override { return eos_flag; }

private:
    source_t& source;
    filter_t& filter;
    bool more = false;
    bool eos_flag = false;

    using filter_output_t = typename Filter<Input, Output>::filter_output_t;

    Output* do_get() override
    {
        if (eos_flag) {
            return nullptr;
        }

        filter_output_t filter_output;
        if (more) {
            filter_output = filter.exchange_item();
        } else {
            do {
                auto item = source.get();
                if (!item) {
                    eos_flag = true;
                    return nullptr;
                }
                filter_output = filter.exchange_item(std::move(item));
            } while (!filter_output.first);
        }
        more = filter_output.second;
        return new Output(*filter_output.first);
        // TODO(Jan): Solve this without the additional alloc/copy operation
    }
};

template <class Input, class Output = Input>
class FilteringSink : public Sink<Input>
{
public:
    using sink_t = Sink<Output>;
    using filter_t = Filter<Input, Output>;

    /// Construct FilteringSink using a given sink and filter
    FilteringSink(sink_t& arg_sink, filter_t& arg_filter)
        : sink(arg_sink), filter(arg_filter)
    {
    }

    void put(std::shared_ptr<const Input> item) override
    {
        typename Filter<Input, Output>::filter_output_t filter_output;
        filter_output = filter.exchange_item(item);
        if (filter_output.first) {
            sink.put(std::move(filter_output.first));
        }
        while (filter_output.second) {
            filter_output = filter.exchange_item();
            if (filter_output.first) {
                sink.put(std::move(filter_output.first));
            }
        }
    }

private:
    sink_t& sink;
    filter_t& filter;
};

class Microslice;
class StorableMicroslice;
using MicrosliceFilter = Filter<Microslice, StorableMicroslice>;
using FilteredMicrosliceSource = FilteredSource<Microslice, StorableMicroslice>;
using FilteringMicrosliceSink = FilteringSink<Microslice, StorableMicroslice>;

} // namespace fles
