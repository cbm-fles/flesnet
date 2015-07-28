// Copyright 2013, 2015 Jan de Cuveland <cmail@cuveland.de>
/// \file
/// \brief Defines the fles::InputArchive template class.
#pragma once

#include "ArchiveDescriptor.hpp"
#include "Source.hpp"
#include <string>
#include <fstream>
#include <memory>
#include <boost/archive/binary_iarchive.hpp>

namespace fles
{

/**
 * \brief The InputArchive class deserializes microslice data sets from an input
 * file.
 */
template <class Base, class Derived, ArchiveType archive_type>
class InputArchive : public Source<Base>
{
public:
    /**
     * \brief Construct an input archive object, open the given archive file for
     * reading, and read the archive descriptor.
     *
     * \param filename File name of the archive file
     */
    InputArchive(const std::string& filename)
    {
        ifstream_ = std::unique_ptr<std::ifstream>(
            new std::ifstream(filename.c_str(), std::ios::binary));
        if (!*ifstream_) {
            throw std::ios_base::failure("error opening file \"" + filename +
                                         "\"");
        }

        iarchive_ = std::unique_ptr<boost::archive::binary_iarchive>(
            new boost::archive::binary_iarchive(*ifstream_));

        *iarchive_ >> descriptor_;

        if (descriptor_.archive_type() != archive_type) {
            throw std::runtime_error("File \"" + filename +
                                     "\" is not of correct archive type");
        }
    }

    /// Delete copy constructor (non-copyable).
    InputArchive(const InputArchive&) = delete;
    /// Delete assignment operator (non-copyable).
    void operator=(const InputArchive&) = delete;

    virtual ~InputArchive(){};

    /// Read the next data set.
    std::unique_ptr<Derived> get()
    {
        return std::unique_ptr<Derived>(do_get());
    };

    /// Retrieve the archive descriptor.
    const ArchiveDescriptor& descriptor() const { return descriptor_; };

private:
    virtual Derived* do_get()
    {
        Derived* sts = nullptr;
        try {
            sts = new Derived();
            *iarchive_ >> *sts;
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

    std::unique_ptr<std::ifstream> ifstream_;
    std::unique_ptr<boost::archive::binary_iarchive> iarchive_;
    ArchiveDescriptor descriptor_;
};

} // namespace fles {
