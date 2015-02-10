#! /usr/bin/perl -w

use strict;
use warnings;

use Config;
$Config{useithreads} or die "This program requires Perl with ithreads";

use threads;
use threads::shared;
use Thread::Queue;
use Fcntl ':mode';

# Thread-safe queue of directories to scan
my $dirScanQueue = Thread::Queue->new();
my($initialDirectory);
my($numThreads) = 50;

my $outputLock :shared = 0; # lock to control stdout
my @threadOutput; # thread-local output buffer
$|=1; # make stdout unbuffered

while (@ARGV) {
    my $arg = shift @ARGV;
    if ($arg eq '-d' && @ARGV) {
        $initialDirectory = shift @ARGV;
        # ensure directory contains a trailing slash
        if ($initialDirectory !~ m|.*/$|) {
            $initialDirectory .= "/";
        }
        printFile(0, $initialDirectory, $initialDirectory);
        $dirScanQueue->enqueue($initialDirectory);
        next;
    }
    if ($arg eq '-t' && @ARGV) {
        $numThreads = shift @ARGV;
        next;
    }
}
flushOutput();

$initialDirectory or die usage($0); # at least 1 dir on the commandline
$numThreads > 0 or die usage($0);
my $idleThreads :shared = 0; # count of threads that are idle
my $shutdown :shared = 0; # boolean; true if we should shutdown the threads

# start the worker threads
my(@threadIds);
for (my $i=0; $i < $numThreads; $i++) {
    $threadIds[$i] = threads->create(\&worker);
}
# collect the workers once they exit
for (my $i=0; $i < $numThreads; $i++) {
    $threadIds[$i]->join();
}

# main worker thread routine
sub worker {
    while (1) {
        my $item = $dirScanQueue->dequeue_nb();
        if (!defined $item) { # empty queue
            lock($idleThreads);
            $idleThreads++;
            # if everyone is idle & there's no more work, we should
            # initiate shutdown
            if ($idleThreads == $numThreads &&
                0 == $dirScanQueue->pending()) { # all idle, no work
                $shutdown = 1;
                cond_broadcast($idleThreads);
            } else {
                cond_wait($idleThreads);
                $idleThreads--;
            }
            last if ($shutdown);
        } else {
            # process the directory we took from the queue
            processDirectory($item);
        }
    }
    flushOutput();
}

# processes a directory by iterating through each entry and printing
# its info. Additionally, for all subdirectories, it enqueues them to
# be scanned.
sub processDirectory {
    my ($dirname) = @_;
    opendir (my $dh, $dirname) ||
        (warn "Unable to open directory: $dirname" && return);
    my ($dummy, $pdir_ino, $rest) = stat($dh);
    while (my $entry = readdir($dh)) {
        next if ($entry eq "." || $entry eq "..");
        my $fullpath = $dirname . $entry;
        my $mode = printFile($pdir_ino, $fullpath, $entry);
        if (defined $mode && S_ISDIR($mode)) {
            # For directories, we need to flush the output queue
            # before we enqueue it for scan to ensure the output
            # file contains the name of the directory before its
            # entries.
            flushOutput();
            enqueue($fullpath . "/");
        }
    }
    closedir $dh;
}

# print the 1-line file info about a full path. Returns the mode field
# for the file.
sub printFile {
    my ($pdir_ino, $fullpath, $filename) = @_;
    my ($dev, $ino, $mode, $nlink, $uid, $gid, $rdev, $size,
        $atime, $mtime, $ctime, $blksize, $blocks) = lstat $fullpath;
    if (defined $mode) {
        #XXX do something about symlinks if (S_ISLNK($mode));
        # properly escape the filename for csv output. Quotes
        # become two quotes and the whole field is then wrapped in
        # quotes.
        $filename =~ s/"/""/g;
        $filename = "\"$filename\"";
        qprint("$pdir_ino,$filename,$ino,$dev,$mode,$nlink,$uid,$gid," .
               "$rdev,$size,$blksize,$blocks,$atime,$mtime,$ctime\n");
    } else {
        print STDERR "File not found: $fullpath\n";
    }
    return $mode;
}

# Put a directory on the queue to be scanned
sub enqueue {
    my ($item) = @_;
    #lock $idleThreads;
    $dirScanQueue->enqueue($item);
    { no warnings 'threads'; cond_signal($idleThreads); }
}

# Buffer output by keeping it in a thread-local array
sub qprint {
    my ($line) = @_;
    push @threadOutput, $line;
    if (@threadOutput > 1000) {
        flushOutput();
    }
}

# Dump the thread-local buffered output
sub flushOutput {
    {
        lock $outputLock;
        print @threadOutput;
    }
    @threadOutput = ();
}

# Print usage information
sub usage {
    my ($progname) = @_;
    return <<EOU
$progname -d <initial_directory> [-t threads]
EOU
;
}
