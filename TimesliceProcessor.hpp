/**
 * \file TimesliceProcessor.hpp
 *
 * 2013, Jan de Cuveland <cmail@cuveland.de>
 */

#ifndef TIMESLICEPROCESSOR_HPP
#define TIMESLICEPROCESSOR_HPP

class TimesliceProcessor
{
public:
    explicit TimesliceProcessor(ComputeBuffer& cb, int index) : _cb(cb), _index(index) {}

    void operator()()
    {
        try {           
            while (true) {
                TimesliceWorkItem wi;
                _cb._work_items.wait_and_pop(wi);
                process(wi);
                TimesliceCompletion c = {wi.ts_pos};
                _cb._completions.push(c);
            }
        }
        catch (concurrent_queue<TimesliceWorkItem>::Stopped) {
            out.trace() << "processor thread " << _index << " done";
        }
    }

    void process(TimesliceWorkItem wi) {
        uint64_t ts_pos = wi.ts_pos;
        uint64_t ts_num = _cb.desc(0).at(ts_pos).ts_num;
        uint64_t ts_size = par->timeslice_size() + par->overlap_size();

        if (out.beTrace()) {
            out.trace() << "processor thread " << _index << " working on timeslice " << ts_num;
        }

        for (size_t in = 0; in < _cb.size(); in++) {
            assert(_cb.desc(0).at(ts_pos).ts_num == ts_num);

            //uint64_t size = _cb.desc(in).at(ts_pos).size;
            uint64_t offset = _cb.desc(in).at(ts_pos).offset;

            const RingBuffer<uint64_t>& data = _cb.data(in);

            uint64_t mc_offset = data.at(offset);
            for (size_t mc = 0; mc < ts_size; mc++) {
                uint64_t this_offset = data.at(offset + mc) - mc_offset + offset + ts_size;
                uint64_t hdr0 = data.at(this_offset + 0);
                uint64_t hdr1 = data.at(this_offset + 1);
                uint64_t mc_size = hdr0 & 0xFFFFFFFF;
                size_t content_words = (mc_size >> 3) - 2;
                uint64_t mc_time = hdr1 & 0xFFFFFFFFFFFF;
                assert(mc_time == ts_num * par->timeslice_size() + mc);

                for (size_t pos = 0; pos < content_words; pos++) {
                    uint64_t this_data = data.at(this_offset + 2 + pos);
                    uint64_t expected = ((uint64_t) in << 48) | pos;
                    assert(this_data == expected);
                }
            }
        }
    }

    ComputeBuffer& _cb;
    int _index;
};


#endif /* TIMESLICEPROCESSOR_HPP */
