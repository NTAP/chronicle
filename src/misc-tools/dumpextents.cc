#include "ChronicleExtents.h"
#include <iostream>

int
main(int argc, char *argv[])
{
    std::cout << EXTENT_CHRONICLE_IP << std::endl
              << EXTENT_CHRONICLE_RPCCOMMON << std::endl
              << EXTENT_CHRONICLE_NFS3_GETATTR << std::endl
              << EXTENT_CHRONICLE_NFS3_SETATTR << std::endl
              << EXTENT_CHRONICLE_NFS3_LOOKUP << std::endl
              << EXTENT_CHRONICLE_NFS3_ACCESS << std::endl
              << EXTENT_CHRONICLE_NFS3_READLINK << std::endl
              << EXTENT_CHRONICLE_NFS3_READWRITE << std::endl
              << EXTENT_CHRONICLE_NFS3_RWCHECKSUM << std::endl
              << EXTENT_CHRONICLE_NFS3_CREATE << std::endl
              << EXTENT_CHRONICLE_NFS3_REMOVE << std::endl
              << EXTENT_CHRONICLE_NFS3_RENAME << std::endl
              << EXTENT_CHRONICLE_NFS3_LINK << std::endl
              << EXTENT_CHRONICLE_NFS3_READDIR << std::endl
              << EXTENT_CHRONICLE_NFS3_READDIR_ENTRIES << std::endl
              << EXTENT_CHRONICLE_NFS3_FSSTAT << std::endl
              << EXTENT_CHRONICLE_NFS3_FSINFO << std::endl
              << EXTENT_CHRONICLE_NFS3_PATHCONF << std::endl
              << EXTENT_CHRONICLE_NFS3_COMMIT << std::endl
              << std::endl;
    return 0;
}
