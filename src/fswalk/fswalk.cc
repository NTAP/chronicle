// -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
/*
 * Copyright (c) 2012 Netapp, Inc.
 * All rights reserved.
 */

/****************************** manpage text *******************************
=head1 NAME

fswalk - walk a directory tree, cataloging its structure

=head1 SYNOPSIS

fswalk B<-p> F<path> [B<-p> F<path> ...] [B<-t> threads] B<-o> F<outfile.ds>

=head1 DESCRIPTION

B<fswalk> is a utility designed to capture a snapshot of a file system
tree. It starts at a provided root directory, recursively descending
the tree and recording information about all files, directories, and
special files that it finds. The resulting information is saved into a
DataSeries file for future analysis.

The information is recorded as a series of records (1 per directory
entry). Each record contains the parent directory's inode number as
well as the filename for the entry and the information that is
returned from a stat(2) call. This provides sufficient information to
recreate a directory tree (excluding any actual file data).

In addition to saving the directory tree, a symlink's "target" is also
saved in a separate table.

=head1 OPTIONS

=over

=item B<-o> F<outfile.ds>

Specifies the name of the DataSeries file to write the scan
information into

=item B<-p> F<path>

Specifies a full pathname to act as the root of a scan. Multiple roots
can be specified. They will be scanned in parallel, with the results
combined into a single output file.

=item B<-t> I<threads>

Specifies the maximum number of threads to use while walking the file
system. This parameter determines the maximum number of outstanding
I/O calls (likely readdir(2) and stat(2) calls). The B<fswalk>
algorithm is able to parallelize the traversal by treating each
discovered directory independently. For an efficient scan, this
parameter should be greater than one, typically in the range of 10 to
50 to ensure the file system is kept busy, though a lower number may
be desired to limit the load on the system.

=back

=head1 EXAMPLES

=over

=over

=item fswalk -p /root/of/the/tree -t 15 -o tree.ds

=back

=back

This walks the directory tree rooted at F</root/of/the/tree>, using up
to 15 simultaneous requests and saving the result of the walk into
F<tree.ds> in the current directory.

=head1 SEE ALSO

chronicle(1)

=head1 COPYRIGHT

Copyright (c) 2012 NetApp, Inc.
All rights reserved.

=cut
***************************************************************************/


#include "Scheduler.h"
#include "DirGatherer.h"
#include "StatGatherer.h"
#include "ScannerFactory.h"
#include <iostream>
#include <cstdlib>

void
usage(std::string progname)
{
    std::cout << progname
              << " -p <path> [-p ...] [-t <threads>] -o <outfile.ds>"
              << std::endl
              << "   -p <path>       : Top of directory tree to scan"
              << std::endl
              << "   -t <threads>    : Number of scanning threads"
              << std::endl
              << "   -o <outfile.ds> : Name of the DataSeries output file"
              << std::endl
              << std::endl;
}

StatGatherer *sg = NULL;
class DGComplete : public DirGatherer::CompletionCb {
public:
    void operator()() {
        if (sg)
            sg->finish(NULL);
    };
};

int
main(int argc, char *argv[])
{
    std::list<std::string> initialPaths;
    unsigned threads = Scheduler::numProcessors();
    std::string filename;

    char opt;
    while ((opt = getopt(argc, argv, "p:t:o:")) > 0) {
        switch (opt) {
        case 'o':
            filename = optarg;
            break;
        case 'p':
            initialPaths.push_back(optarg);
            break;
        case 't':
            threads = atoi(optarg);
            break;
        default:
            assert(0);
        }
    }

    if ("" == filename) {
        usage(argv[0]);
        std::cout << "Error: Must supply an output filename" << std::endl
                  << std::endl;
        exit(EXIT_FAILURE);
    }
    if (initialPaths.empty()) {
        usage(argv[0]);
        std::cout << "Error: Must supply at least 1 path to scan" << std::endl
                  << std::endl;
        exit(EXIT_FAILURE);
    }

    Scheduler::initScheduler();
    ScannerFactory f;
    DirGatherer *d = new DirGatherer(&f);
    f.dirGatherer(d);
    sg = new StatGatherer(filename);
    f.statGatherer(sg);

    d->maxScanners(threads);
    for (std::list<std::string>::const_iterator i = initialPaths.begin();
         i != initialPaths.end(); ++i) {
        d->addDirectory(*i);
    }
    DGComplete dgcb;
    d->start(&dgcb);
    Scheduler::startSchedulers(threads, d, false);
    std::cout << "all done" << std::endl;
    return EXIT_SUCCESS;
}
