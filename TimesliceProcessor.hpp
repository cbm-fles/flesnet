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
        uint64_t ts_num = _cb.get_desc(0, ts_pos).ts_num;
        uint64_t ts_size = par->timeslice_size() + par->overlap_size();

        if (out.beDebug()) {
            out.debug() << "processor thread " << _index << " working on timeslice " << ts_num;
        }

        for (size_t in = 0; in < _cb.size(); in++) {
            TimesliceComponentDescriptor& desc = _cb.get_desc(in, ts_pos);
            assert(desc.ts_num == ts_num);

            MicrosliceDescriptor* mc_desc =
                &reinterpret_cast<MicrosliceDescriptor&>(_cb.get_data(in, desc.offset));

            uint8_t* mc_data =
                &_cb.get_data(in, desc.offset) + ts_size * sizeof(MicrosliceDescriptor);

            for (size_t mc = 0; mc < ts_size; mc++) {
                assert(mc_desc[mc].idx == ts_num * par->timeslice_size() + mc);
                uint64_t* this_mc_data =
                    reinterpret_cast<uint64_t*>(mc_data + mc_desc[mc].offset - mc_desc[0].offset);
                if (check_pattern) {
                    uint32_t crc = 0x00000000;
                    for (size_t pos = 0; pos < mc_desc[mc].size; pos += sizeof(uint64_t)) {
                        uint64_t data_word = this_mc_data[pos / sizeof(uint64_t)];
                        crc ^= (data_word & 0xffffffff) ^ (data_word >> 32);
                        uint64_t expected = ((uint64_t) in << 48) | pos;
                        assert(data_word == expected);
                    }
                    assert(crc == mc_desc[mc].crc);
                }
            }
        }
    }

    ComputeBuffer& _cb;
    int _index;
};


#endif /* TIMESLICEPROCESSOR_HPP */
