#ifndef OPTIONSGROUP_HPP
#define OPTIONSGROUP_HPP

// \todo: Mention in documentation, why mentioning the defaults in the
// doxygen documentation is not feasible and additionally provide a some
// reasoning why this may not even be a bad thing.

class optionsGroup {

public:
  virtual boost::program_options::options_description
  optionsDescription(bool hidden) = 0;
  virtual ~optionsGroup() = 0;
};

inline optionsGroup::~optionsGroup() {}

#endif // OPTIONSGROUP_HPP
