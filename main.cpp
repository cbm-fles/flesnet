/*
 * main.cpp
 *
 * 2012, Jan de Cuveland
 */

#include <cstdlib>

#include "Application.hpp"


int 
main(int argc, char* argv[])
{
    try {
        Parameters par(argc, argv);

        if (par.node_type() == Parameters::INPUT_NODE) {
            InputApplication app(par);
            app.run();
        } else {
            ComputeApplication app(par);
            app.run();
        }
    }
    catch (std::exception const& e) {
        std::cerr << "error: " << e.what() << "\n";
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}
