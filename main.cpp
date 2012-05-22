/*
 * \file main.cpp
 *
 * 2012, Jan de Cuveland
 */

#include <cstdlib>

#include "Application.hpp"
#include "global.hpp"

einhard::Logger<einhard::ALL, true> Log;
Parameters* Par;

int
main(int argc, char* argv[])
{
    try {
        Par = new Parameters(argc, argv);
        Par->selectDebugValues();

        if (Par->nodeType() == Parameters::INPUT_NODE) {
            InputApplication app(*Par);
            app.run();
        } else {
            ComputeApplication app(*Par);
            app.run();
        }
    } catch (std::exception const& e) {
        std::cerr << "error: " << e.what() << "\n";
        return EXIT_FAILURE;
    }

    delete Par;
    return EXIT_SUCCESS;
}
