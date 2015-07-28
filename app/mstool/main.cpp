// Copyright 2012-2013 Jan de Cuveland <cmail@cuveland.de>

#include "Parameters.hpp"
#include "Application.hpp"

int main(int argc, char* argv[])
{
    try {
        Parameters par(argc, argv);
        Application app(par);
        app.run();
    } catch (std::exception const& e) {
        std::cerr << "FATAL: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
