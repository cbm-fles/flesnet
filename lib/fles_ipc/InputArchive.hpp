// Copyright 2013, 2015 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include "ArchiveDescriptor.hpp"
#include "Source.hpp"
#include <string>
#include <fstream>
#include <memory>
#include <boost/archive/binary_iarchive.hpp>

namespace fles
{

//! The InputArchive deserializes data sets from an input file.
template <class Base, class Derived, ArchiveType archive_type>
class InputArchive : public Source<Base>
{
public:
    InputArchive(const std::string& filename)
    {
        _ifstream = std::unique_ptr<std::ifstream>(
            new std::ifstream(filename.c_str(), std::ios::binary));
        if (!*_ifstream) {
            throw std::ios_base::failure("error opening file \"" + filename +
                                         "\"");
        }

        _iarchive = std::unique_ptr<boost::archive::binary_iarchive>(
            new boost::archive::binary_iarchive(*_ifstream));

        *_iarchive >> _descriptor;

        if (_descriptor.archive_type() != archive_type) {
            throw std::runtime_error("File \"" + filename +
                                     "\" is not of correct archive type");
        }
    }

    InputArchive(const InputArchive&) = delete;
    void operator=(const InputArchive&) = delete;

    virtual ~InputArchive(){};

    /// Read the next data set.
    std::unique_ptr<Derived> get()
    {
        return std::unique_ptr<Derived>(do_get());
    };

    const ArchiveDescriptor& descriptor() const { return _descriptor; };

private:
    virtual Derived* do_get()
    {
        Derived* sts = nullptr;
        try {
            sts = new Derived();
            *_iarchive >> *sts;
        } catch (boost::archive::archive_exception e) {
            if (e.code ==
                boost::archive::archive_exception::input_stream_error) {
                delete sts;
                return nullptr;
            }
            throw;
        }
        return sts;
    }

    std::unique_ptr<std::ifstream> _ifstream;
    std::unique_ptr<boost::archive::binary_iarchive> _iarchive;
    ArchiveDescriptor _descriptor;
};

} // namespace fles {
