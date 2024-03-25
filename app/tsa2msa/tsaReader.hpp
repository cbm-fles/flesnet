#ifndef TSAREADER_HPP
#define TSAREADER_HPP

/**
 * @class tsaReader
 * @brief This class represents a reader for tsa archives.
 * 
 * The tsaReader provides functionality to read TSA data from a file or
 * other input stream. For now, many default methods such as
 * copy constructor, copy assignment, move constructor, and move
 * assignment are deleted until they are needed (if ever). Similarly,
 * the class is marked as final to prevent inheritance (until needed).
*/
class tsaReader final {
public:
  /**
   * @brief Constructor for the tsaReader object using default options.
   *
   */
  tsaReader() = default;

  /**
   * @brief Destructor for the tsaReader object.
   */
  ~tsaReader() = default;

  // Delete copy constructor:
  tsaReader(const tsaReader& other) = delete;

  // Delete copy assignment:
  tsaReader& operator=(const tsaReader& other) = delete;

  // Delete move constructor:
  tsaReader(tsaReader&& other) = delete;

  // Delete move assignment:
  tsaReader& operator=(tsaReader&& other) = delete;

private:

};

#endif // TSAREADER_HPP