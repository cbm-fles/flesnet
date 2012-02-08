/*
 * ibtsb.cpp
 *
 * 2012, Jan de Cuveland
 */

#include <iostream>
#include <cstdlib>

#include "Parameters.hpp"

int 
main(int argc, char* argv[])
{
    try {
        Parameters par(argc, argv);

        if (par.node_type() == Parameters::INPUT_NODE) {
            std::cout << "i";
        } else {
            std::cout << "p";
        }
        //run_server(argc, argv);
        //run_client(argc, argv);
    }
    catch (std::exception const& e) {
        std::cerr << "error: " << e.what() << "\n";
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}
