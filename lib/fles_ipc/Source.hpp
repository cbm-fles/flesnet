// Copyright 2013, 2015 Jan de Cuveland <cmail@cuveland.de>
/// \file
/// \brief Defines the fles::Source template class.
#pragma once

#include <memory>

namespace fles {

/**
 * \brief The Source class implements the generic item-based input interface.
 *
 * This class is an abstract base class for several classes using an item-based
 * input interface.
 */
template <class T> class Source {
public:
  /**
   * \brief Retrieve the next item.
   *
   * This function blocks if the next item is not yet available. If the
   * end-of-stream condition is met, a nullptr is returned for any subsequent
   * calls to this function.
   *
   * \return pointer to the item, or nullptr if end-of-stream
   */
  std::unique_ptr<T> get() { return std::unique_ptr<T>(do_get()); };

  virtual bool eos() const = 0;

  virtual ~Source() = default;

private:
  virtual T* do_get() = 0;
};

} // namespace fles
