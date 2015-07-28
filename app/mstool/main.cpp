// Copyright 2012-2015 Jan de Cuveland <cmail@cuveland.de>

#include "Parameters.hpp"
#include "Application.hpp"
#include "log.hpp"

int main(int argc, char* argv[])
{
    try {
        Parameters par(argc, argv);
        Application app(par);
        app.run();
    } catch (std::exception const& e) {
        L_(fatal) << e.what();
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
