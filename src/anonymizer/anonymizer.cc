// -*- mode: C++; c-basic-offset: 4; tab-width: 4; c-indent-tabs-mode: nil -*-
/*
 * Copyright (c) 2013 Netapp, Inc.
 * All rights reserved.
 */

/****************************** manpage text ******************************
=head1 NAME

anonymizer - anonymize a DataSeries file

=head1 SYNOPSIS

anonymizer B<-i> F<input.ds> B<-o> F<output.ds> B<-k> F<keyfile>
B<[options]>

=head1 DESCRIPTION

B<anonymizer> is a utility that is designed to apply a set of
anonymization operations to a DataSeries file, producing a new,
anonymized version of the input.

The anonymization operations are designed to be general and extensible
so that the same program can be used to anonymize many different
traces. It works by substituting the values for particular fields with
an anonymized version of the original data. See the options below for
a list of the different types of field anonymization that are
supported.

=head1 OPTIONS

=over

=item B<-B>, B<--buffer>

Insert a DataSeries buffer into the processing pipeline. Buffer
modules allow the parts of the pipeline before and after to run in
parallel to increase processing speed.

=item B<-C>, B<--constant=>I<ExtentName>,I<FieldName>,I<Value>

Replaces the named field with a constant. For fields that are
"nullable," the correct option is probable to use B<--null> to
anonymize the field.

Valid for: C<byte>, C<int32>, C<int64>, C<variable32>

=item B<-c>, B<--compression=>I<Type>[,I<...>]

Specify the permitted types of compression for the output file. By
default, all algorithms are enabled. This option can be used to force
a particular one to be used or disallow one by specifying all except
the disallowed. Avaliable compression types are: C<bz2>, C<lzo>,
C<lzf>, C<zlib>, or C<none>.

=item B<-E>, B<--fpe=>I<ExtentName>,I<FieldName>,I<tweak>

Anonymizes a field using format-preserving encryption. This is designed to
be used when the size of the output data should match the size of the input data. This is particularly useful for the fixed-size fields.

Valid for: C<int32>, C<int64>

=item B<-F>, B<--filename_ext=>I<ExtentName>,I<FieldName>

Anonymize a filename, preserving the original name length and
extension. Filenames are expected to be in the format C<basename.ext>
where either portion may be empty, and the extension is any remaining
text after the final dot in the name. The anonymized name has three
parts: the original length, the hashed base filename, and the
cleartext extension.

Example:

The filename, C<myfile.txt>, could hash to:
C<000a|9bf908850d59279e7ff1513cee29e4d30c79a8d5|txt>. The original
length was 10 characters (hex 0x000a), C<file> hashed to C<9b...d5>,
and the extension remained in the clear.

Valid for: C<variable32>

=item B<-H>, B<--hmac=>I<ExtentName>,I<FieldName>

Replaces a field's contents with its HMAC. While this can be used on
fixed-size fields, it is not recommended since the hash value will
simply be truncated to fit the required size. This is likely to cause
an unacceptable number of hash collisions in the output data. It is
only recommended for use on C<variable32> fields, where the contents
will be replaced by a fixed-size HMAC hash.

Valid for: C<byte>, C<int32>, C<int64>, C<variable32>

=item B<-I>, B<--ipaddr=>I<ExtentName>,I<FieldName>,I<MaskBits>

Anonymize an IP addresses by masking the top I<MaskBits> of the
address and mapping them into the 10.0.0.0 IP range. The typical usage
would be to mask the IP address at the level of the organization's IP
address allocation. For example, an organization who owned a class B
set of addresses should probably be masked at 16 bits.

IP addresses are expected to be stored in an C<int32> field in host
byte order.

Masking example:

192.168.1.150 masked with 16 bits would produce 10.0.1.150.

=item B<-i>, B<--infile=>F<input.ds>

Specifies the name of the DataSeries file to anonymize

=item B<-k>, B<--keyfile=>F<key_file>

Specifies the name of a file from which to generate a key for use in
the anonymization methods that are based on hashing. The contents of
the file are read in and hashed to produce the key which is then used
for all HMAC calculations. Using the same keyfile for multiple
DataSeries files will cause the same cleartext to map to the same
anonymized values.

=item B<-N>, B<--null=>I<ExtentName>,I<FieldName>

For fields that are marked as "nullable," anonymize them by setting
their value to null.

Valid for: C<byte>, C<int32>, C<int64>, C<variable32>

=item B<-o>, B<--outfile=>F<output.ds>

Specifies the name of the DataSeries file to write the anonymized data
into.

=item B<-P>, B<--pathname_ext=>I<ExtentName>,I<FieldName>

Anonymize a field that is composed of a pathname. The pathname is
split up by component (separator = "/") and each component is
anonymized using the same algorithm as B<--filename_ext>,
above. Filename fields that have been anonymized with
B<--filename_ext> can be correlated to pathnames anonymized with this
option. The separator is passed through, as are components that are exactly C<.> or C<..>.

Valid for: C<variable32>

=item B<-S>, B<--strip=>I<ExtentName>

Normally, all extents are passed through from the input file to the
output file, whether anonymized or not. This option can be used to
strip a whole extent type from the anonymized file.

=back

=head1 EXAMPLES

Anonymize filenames and symlink targets in a DataSeries file generated by fswalk(1):

=over

=over

=item anonymizer -i infile.ds -o outfile.ds -k keyfile --filename_ext=fssnapshot_stat,filename --pathname_ext=fssnapshot_symlink,target

=back

=back

=head1 SEE ALSO

chronicle(1), fswalk(1)

=head1 COPYRIGHT

Copyright (c) 2013 NetApp, Inc.
All rights reserved.

=cut
**************************************************************************/

#include <cstdlib>
#include <getopt.h>
#include <iostream>
#include <list>
#include <string>

#include "AnonHelper.h"
#include "DataSeries/PrefetchBufferModule.hpp"
#include "DataSeries/SequenceModule.hpp"
#include "DataSeries/TypeIndexModule.hpp"
#include "ConstAnonymizer.h"
#include "ExtentStrip.h"
#include "FileExtAnonymizer.h"
#include "FpeAnonymizer.h"
#include "IpAnonymizer.h"
#include "NullAnonymizer.h"
#include "PathAnonymizer.h"

typedef std::list<std::string> StringList;
std::string commandName;


// The size of memory buffer for EACH buffer module
static const unsigned BUFFER_SIZE = 128 * 1024 * 1024;

static const struct option OPTIONS[] = {
    {"help", no_argument, 0, 'h'},
    {"infile", required_argument, 0, 'i'},
    {"keyfile", required_argument, 0, 'k'},
    {"outfile", required_argument, 0, 'o'},
    {"buffer", no_argument, 0, 'B'},
    {"constant", required_argument, 0, 'C'},
    {"compression", required_argument, 0, 'c'},
    {"fpe", required_argument, 0, 'E'},
    {"hmac", required_argument, 0, 'H'},
    {"filename_ext", required_argument, 0, 'F'},
    {"ipaddr", required_argument, 0, 'I'},
    {"null", required_argument, 0, 'N'},
    {"pathname_ext", required_argument, 0, 'P'},
    {"strip", required_argument, 0, 'S'},
    {0, 0, 0, 0} // denotes end of list
};

static void
usage()
{
    std::cout << std::endl << "Usage summary:" << std::endl << std::endl
              << commandName
              << " REQUIRED_OPTIONS... [OPTIONAL_OPTIONS...]"
              << std::endl << std::endl << "\t-h, --help\t\t\t\t\t"
              << "this help/usage information"
        // required options
              << std::endl << std::endl << "\tRequired options:"
              << std::endl << "\t-i, --infile=FILE\t\t\t\t"
              << "input DataSeries file to process (may be repeated)"
              << std::endl << "\t-k, --keyfile=FILE\t\t\t\t"
              << "file containing key to be used for anonymization"
              << std::endl << "\t-o, --outfile=FILE\t\t\t\t"
              << "output DataSeries file to write"
        // optional options
              << std::endl << std::endl << "\tOptional options:"
              << std::endl << "\t-B, --buffer\t\t\t\t\t"
              << "insert a DataSeries buffer module in the pipeline"
              << std::endl << "\t-C, --constant=EXTENT,FIELD,VALUE\t"
              << "set a field to a constant value"
              << std::endl << "\t-c, --compression=TYPE,...\t\t\t"
              << "compression for the output file: bz2, lzo, lzf, zlib, none, all (default)"
              << std::endl << "\t-E, --fpe=EXTENT,FIELD,TWEAK\t\t\t"
              << "anonymize a 32/64-bit field w/ format preseving encryption"
              << std::endl << "\t-F, --filename_ext=EXTENT,FIELD\t\t\t"
              << "anonymize a filename field, splitting out the extension"
              << std::endl << "\t-H, --hmac=EXTENT,FIELD\t\t\t\t"
              << "anonymize a field via HMAC"
              << std::endl << "\t-I, --ipaddr=EXTENT,FIELD,BITS\t\t\t"
              << "mask off the top BITS of an IP address field"
              << std::endl << "\t-N, --null=EXTENT,FIELD\t\t\t\t"
              << "anonymize a (nullable) field by setting it to null"
              << std::endl << "\t-P, --pathname_ext=EXTENT,FIELD\t\t\t"
              << "anonymize a pathname field, using filename_ext on each component"
              << std::endl << "\t-S, --strip=EXTENT\t\t\t\t"
              << "remove an extent from the stream"
              << std::endl << std::endl;
}

static std::string
optionToGetoptString(const struct option *options)
{
    std::string optstring;
    for (const struct option *o = options;
         o->name || o->has_arg || o->flag || o->val; ++o) {
        // process it if getopt_long will return it
        if (0 == o->flag) {
            optstring += o->val;
            switch (o->has_arg) {
            case required_argument:
                optstring += ":";
                break;
            case optional_argument:
                optstring += "::";
                break;
            }
        }
    }
    return optstring;
}

static bool
isBuiltinType(ExtentType::Ptr et)
{
    return (et->getName() == "DataSeries: ExtentIndex" ||
            et->getName() == "DataSeries: XmlType" ||
            (et->getName() == "Info::DSRepack" &&
             et->getNamespace() == "ssd.hpl.hp.com"));
}

static void
setupLibrary(StringList infiles, DataSeriesSink *sink)
{
    ExtentTypeLibrary lib;

    // iterate over the files
    for (StringList::const_iterator i = infiles.begin();
         i != infiles.end(); ++i) {
        DataSeriesSource src(*i);

        // iterate over the extent types in the file
        for (ExtentTypeLibrary::NameToType::iterator j =
                 src.getLibrary().name_to_type.begin();
             j != src.getLibrary().name_to_type.end(); ++j) {
            ExtentType::Ptr type = j->second;

            // make sure it's not a built-in type
            if (!isBuiltinType(type)) {
                const ExtentType::Ptr et =
                    lib.getTypeByNamePtr(j->first, true);
                // Make sure the ET doesn't have different definitions
                // in different files
                assert("Looks like an ExtentType is defined differently in "
                       "the files" && (!et || et == j->second));
                // hasn't already been copied
                if (!et) {
                    lib.registerTypePtr(j->second->getXmlDescriptionString());
                }
            }
        }
    }

    sink->writeExtentLibrary(lib);
}

void
commandError(std::string message)
{
    std::cout << std::endl << "ERROR: " << message << std::endl;
    usage();
    exit(EXIT_FAILURE);
}

static std::string
gAndP(StringList &sl)
{
    std::string val = sl.front();
    sl.pop_front();
    return val;
}

static int
getCompressionMode(std::string arg)
{
    StringList args = tokenize(arg, ',');
    int compression = 0;
    for (StringList::const_iterator i = args.begin(); i != args.end(); ++i) {
        if (*i == "lzo") {
            compression |= Extent::compression_algs[Extent::compress_mode_lzo].compress_flag;
        } else if (*i == "zlib") {
            compression |= Extent::compression_algs[Extent::compress_mode_zlib].compress_flag;
        } else if (*i == "bz2") {
            compression |= Extent::compression_algs[Extent::compress_mode_bz2].compress_flag;
        } else if (*i == "lzf") {
            compression |= Extent::compression_algs[Extent::compress_mode_lzf].compress_flag;
        } else if (*i == "all") {
            compression |= Extent::compress_all;
        } else if (*i == "none") {
            compression = Extent::compression_algs[Extent::compress_mode_none].compress_flag;
        } else {
            commandError("Unknown compression algorithm");
        }
    }
    return compression;
}

static void
addConstAnonymizer(SequenceModule *pipeline, std::string args)
{
    StringList optionArgs = tokenize(args, ',');
    if (optionArgs.size() != 3)
        commandError("bad arguments for constant anonymizer");
    std::string constant = optionArgs.back();
    Anonymizer *anonymizer = new ConstAnonymizer(std::stoull(constant));
    pipeline->addModule(new GenericFieldAnonymizer(&pipeline->tail(),
                                                   gAndP(optionArgs),
                                                   gAndP(optionArgs),
                                                   anonymizer));
}

static void
addFileExtAnonymizer(SequenceModule *pipeline, std::string args,
                     std::string keyfile)
{
    StringList optionArgs = tokenize(args, ',');
    if (optionArgs.size() != 2)
        commandError("bad arguments for file_ext anonymizer");
    Anonymizer *hasher = new HashAnonymizer(keyfile);
    Anonymizer *anonymizer = new FileExtAnonymizer(hasher);
    pipeline->addModule(new GenericFieldAnonymizer(&pipeline->tail(),
                                                   gAndP(optionArgs),
                                                   gAndP(optionArgs),
                                                   anonymizer));
}

static void
addHmacAnonymizer(SequenceModule *pipeline, std::string args,
                  std::string keyfile)
{
    StringList optionArgs = tokenize(args, ',');
    if (optionArgs.size() != 2)
        commandError("bad arguments for HMAC anonymizer");
    Anonymizer *anonymizer = new HashAnonymizer(keyfile);
    pipeline->addModule(new GenericFieldAnonymizer(&pipeline->tail(),
                                                   gAndP(optionArgs),
                                                   gAndP(optionArgs),
                                                   anonymizer));
}

static void
addFpeAnonymizer(SequenceModule *pipeline, std::string args,
                 std::string keyfile)
{
    StringList optionArgs = tokenize(args, ',');
    if (optionArgs.size() != 3)
        commandError("bad arguments for FPE anonymizer");
    std::string extent = gAndP(optionArgs);
    std::string field = gAndP(optionArgs);
    std::string tweak = gAndP(optionArgs);
    Anonymizer *anonymizer = new FpeAnonymizer(keyfile,
                                               tweak);
    pipeline->addModule(new GenericFieldAnonymizer(&pipeline->tail(),
                                                   extent,
                                                   field,
                                                   anonymizer));
}

static void
addIpAnonymizer(SequenceModule *pipeline, std::string args)
{
    StringList optionArgs = tokenize(args, ',');
    if (optionArgs.size() != 3)
        commandError("bad arguments for IP anonymizer");
    std::string mask = optionArgs.back();
    Anonymizer *anonymizer = new IpAnonymizer(std::stoul(mask));
    pipeline->addModule(new GenericFieldAnonymizer(&pipeline->tail(),
                                                   gAndP(optionArgs),
                                                   gAndP(optionArgs),
                                                   anonymizer));
}

static void
addNullAnonymizer(SequenceModule *pipeline, std::string args)
{
    StringList optionArgs = tokenize(args, ',');
    if (optionArgs.size() != 2)
        commandError("bad arguments for null anonymizer");
    pipeline->addModule(new NullAnonymizer(&pipeline->tail(),
                                           gAndP(optionArgs),
                                           gAndP(optionArgs)));
}

static void
addPathExtAnonymizer(SequenceModule *pipeline, std::string args,
                     std::string keyfile)
{
    StringList optionArgs = tokenize(args, ',');
    if (optionArgs.size() != 2)
        commandError("bad arguments for file_ext anonymizer");
    Anonymizer *hasher = new HashAnonymizer(keyfile);
    Anonymizer *fanon = new FileExtAnonymizer(hasher);
    Anonymizer *anonymizer = new PathAnonymizer(fanon);
    pipeline->addModule(new GenericFieldAnonymizer(&pipeline->tail(),
                                                   gAndP(optionArgs),
                                                   gAndP(optionArgs),
                                                   anonymizer));
}

static void
addStripAnonymizer(SequenceModule *pipeline, std::string args)
{
    StringList optionArgs = tokenize(args, ',');
    if (optionArgs.size() != 1)
        commandError("bad arguments for strip anonymizer");
    pipeline->addModule(new ExtentStrip(&pipeline->tail(),
                                        gAndP(optionArgs)));
}

std::pair<std::string, std::string>
optionPair(std::string mName, std::string argString)
{
    return std::make_pair(mName, argString);
}

int
main(int argc, char *argv[])
{
    commandName = argv[0];

    TypeIndexModule *source = new TypeIndexModule();
    // Sequence of anonymization modules we will be using
    SequenceModule *pipeline = new SequenceModule(source);
    StringList infileNames;
    std::string outfileName;
    std::string keyfileName;
    int compressionMode = Extent::compress_all;
    std::list<std::pair<std::string, std::string> > moduleList;

    std::string optString = optionToGetoptString(OPTIONS);
    int optIndex, opt;
    while ((opt = getopt_long(argc, argv, optString.c_str(), OPTIONS,
                              &optIndex)) != -1) {
        switch (opt) {
        case 'B':
            pipeline->addModule(new PrefetchBufferModule(pipeline->tail(),
                                    BUFFER_SIZE));
            break;
        case 'C':
            moduleList.push_back(optionPair("const", optarg));
            break;
        case 'c':
            compressionMode = getCompressionMode(optarg);
            break;
        case 'E':
            moduleList.push_back(optionPair("fpe", optarg));
            break;
        case 'F':
            moduleList.push_back(optionPair("file_ext", optarg));
            break;
        case 'H':
            moduleList.push_back(optionPair("hmac", optarg));
            break;
        case 'h':
            usage();
            exit(EXIT_SUCCESS);
        case 'I':
            moduleList.push_back(optionPair("ip", optarg));
            break;
        case 'i':
            infileNames.push_back(std::string(optarg));
            break;
        case 'k':
            keyfileName = optarg;
            break;
        case 'o':
            outfileName = optarg;
            break;
        case 'N':
            moduleList.push_back(optionPair("null", optarg));
            break;
        case 'P':
            moduleList.push_back(optionPair("path_ext", optarg));
            break;
        case 'S':
            moduleList.push_back(optionPair("strip", optarg));
            break;
        default:
            usage();
            exit(EXIT_FAILURE);
        }
    }

    // validate arguments
    if (outfileName == "") {
        commandError("Missing output file argument");
    }
    if (infileNames.empty()) {
        commandError("Missing input file argument(s)");
    }
    if (keyfileName == "") {
        commandError("Missing key file argument");
    }

    // add the modules to the pipeline
    for (std::list<std::pair<std::string, std::string> >::const_iterator i =
             moduleList.begin(); i != moduleList.end(); ++i) {
        if (i->first == "const")
            addConstAnonymizer(pipeline, i->second);
        else if (i->first == "null")
            addNullAnonymizer(pipeline, i->second);
        else if (i->first == "file_ext")
            addFileExtAnonymizer(pipeline, i->second, keyfileName);
        else if (i->first == "fpe")
            addFpeAnonymizer(pipeline, i->second, keyfileName);
        else if (i->first == "hmac")
            addHmacAnonymizer(pipeline, i->second, keyfileName);
        else if (i->first == "ip")
            addIpAnonymizer(pipeline, i->second);
        else if (i->first == "path_ext")
            addPathExtAnonymizer(pipeline, i->second, keyfileName);
        else if (i->first == "strip")
            addStripAnonymizer(pipeline, i->second);
        else
            assert("unknown module type" && 0);
    }

    DataSeriesSink *sink = new DataSeriesSink(outfileName, compressionMode);
    sink->setMaxBytesInProgress(8 * BUFFER_SIZE);

    // Copy the extent descriptions to the output file
    setupLibrary(infileNames, sink);

    // Add the source files to be read
    for (StringList::const_iterator i = infileNames.begin();
         i != infileNames.end(); ++i) {
        source->addSource(*i);
    }
    source->startPrefetching(BUFFER_SIZE, 8 * BUFFER_SIZE);

    DataSeriesSink::Stats stats;

    std::cout << "Writing extents " << std::flush;
    while (Extent::Ptr e = pipeline->getSharedExtent()) {
        ExtentType::Ptr et = e->getTypePtr();
        if (!isBuiltinType(et)) {
            sink->writeExtent(*e, &stats);
            std::cout << "." << std::flush;
        }
    }
    std::cout << std::endl;
    std::cout << "Decompressor wait fraction: " << source->waitFraction()
              << std::endl;

    delete pipeline;
    delete sink;

    stats.printText(std::cout);

    return EXIT_SUCCESS;
}
