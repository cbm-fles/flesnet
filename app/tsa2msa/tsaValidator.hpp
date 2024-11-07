#ifndef TSAVALIDATOR_HPP
#define TSAVALIDATOR_HPP

/**
 * @class tsaValidator
 * @brief This class validates the Timeslices read by a tsaReader.
 *
 * @detailed The tsaValidator provides functionality to validate
 * the Timeslices read by a tsaReader. In particular, it checks whether
 * the Timeslices are in chronological order and whether the number of
 * core microslices and components is constant.
 */
class tsaValidator {
private:
  // true until proven false:
  bool time_monotonically_increasing;
  bool time_strict_monotonicity;
  bool index_monotonically_increasing;
  bool index_strict_monotonicity;
  bool num_core_microslices_constant;
  bool num_components_constant;

  // number of violations:
  uint64_t nStrictMonotonicityViolationsTime;
  uint64_t nStrictMonotonicityViolationsIndex;
  uint64_t nMonotonicityViolationsTime;
  uint64_t nMonotonicityViolationsIndex;
  uint64_t nNumCoreMicroslicesViolations;
  uint64_t nNumComponentsViolations;

  // last values, used for comparison with current values:
  uint64_t last_timeslice_start_time;
  uint64_t last_timeslice_index;
  uint64_t last_timeslice_num_core_microslices;
  uint64_t last_timeslice_num_components;
  uint64_t nTimeslices;

public:
  /**
   * @brief Constructor for the tsaValidator object.
   */
  tsaValidator();

  /**
   * @brief Destructor for the tsaValidator object.
   */
  ~tsaValidator() = default;

  /**
   * @brief Validates the Timeslices read by a tsaReader and their
   * order.
   *
   * \todo Reconsider the return value.
   *
   * @return EX_OK if successful, EX_SOFTWARE if an error occurred.
   */
  int validate(const std::unique_ptr<fles::Timeslice>& timeslice);

  void printVerboseIntermediateResult();

  void printSummary();

private:
  // Delete copy constructor:
  tsaValidator(const tsaValidator& other) = delete;

  // Delete copy assignment:
  tsaValidator& operator=(const tsaValidator& other) = delete;

  // Delete move constructor:
  tsaValidator(tsaValidator&& other) = delete;

  // Delete move assignment:
  tsaValidator& operator=(tsaValidator&& other) = delete;
};

#endif // TSAVALIDATOR_HPP
