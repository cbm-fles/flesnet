/**
 * \file TimesliceProcessor.hpp
 *
 * 2013, Jan de Cuveland <cmail@cuveland.de>
 */

#ifndef TIMESLICEPROCESSOR_HPP
#define TIMESLICEPROCESSOR_HPP

class TimesliceProcessor : public ThreadContainer
{
public:
    explicit TimesliceProcessor(ComputeBuffer& cb, int index) : _cb(cb), _index(index) {}

    void operator()()
    {
        //set_cpu(2 + _index);

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
        bool check_pattern = par->check_pattern();

        uint64_t ts_pos = wi.ts_pos;
        uint64_t ts_num = _cb.desc(0).at(ts_pos).ts_num;
        uint64_t ts_size = par->timeslice_size() + par->overlap_size();
        if (out.beDebug()) {
            out.debug() << "processor thread " << _index << " working on timeslice " << ts_num;
        }

        for (size_t in = 0; in < _cb.size(); in++) {
            assert(_cb.desc(0).at(ts_pos).ts_num == ts_num);

            //uint64_t size = _cb.desc(in).at(ts_pos).size;
            uint64_t offset = _cb.desc(in).at(ts_pos).offset;

            const RingBuffer<>& data = _cb.data(in);

            uint64_t mc_offset = ((MicrosliceDescriptor&) data.at(offset)).offset;
            for (size_t mc = 0; mc < ts_size; mc++) {
                uint64_t this_offset = ((MicrosliceDescriptor&) data.at(offset + mc * sizeof(MicrosliceDescriptor))).offset - mc_offset + offset + ts_size * sizeof(MicrosliceDescriptor);
                uint64_t hdr0 = (uint64_t&) data.at(this_offset + 0);
                uint64_t hdr1 = (uint64_t&) data.at(this_offset + sizeof(uint64_t));
                uint64_t mc_size = hdr0 & 0xFFFFFFFF;
                size_t content_bytes = mc_size - 2 * 8;
                uint64_t mc_time = hdr1 & 0xFFFFFFFFFFFF;
                assert(mc_time == ts_num * par->timeslice_size() + mc);

                if (check_pattern) {
                    for (size_t pos = 0; pos < content_bytes; pos += sizeof(uint64_t)) {
                        uint64_t this_data = (uint64_t &) data.at(this_offset + 2 * 8 + pos);
                        uint64_t expected = ((uint64_t) in << 48) | pos;
                        assert(this_data == expected);
                    }
                }
            }
        }
    }

    ComputeBuffer& _cb;
    int _index;
};


#endif /* TIMESLICEPROCESSOR_HPP */
