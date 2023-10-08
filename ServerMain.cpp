#include "ApplicationServer.hpp"

i32 main(i32 argc, char** argv)
{
    ApplicationServer().Run(std::cin);
    return 0;
}