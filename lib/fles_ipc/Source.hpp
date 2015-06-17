// Copyright 2013, 2015 Jan de Cuveland <cmail@cuveland.de>
#pragma once

#include <memory>

namespace fles
{

/**
 * \brief The Source class implements the generic item-based input interface.
 *
 * This class is an abstract base class for several classes using an item-based
 * input interface.
 */
template <class T> class Source
{
public:
    /**
     * \brief Retrieve the next item.
     *
     * This function blocks if the next item is not yet available.
     *
     * \return pointer to the item, or nullptr if end-of-file
     */
    std::unique_ptr<T> get() { return std::unique_ptr<T>(do_get()); };

    virtual ~Source(){};

protected:
    /// End-of-file flag.
    bool _eof = false;

private:
    virtual T* do_get() = 0;
};

} // namespace fles {
