#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <getopt.h>

#include <cstdint>
#include <iostream>
#include <cstdlib>
#include <vector>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/ordered_index_fwd.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/member.hpp>

#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include "MurmurHash3.h"
#include <pthread.h>
#include <time.h>
#include <limits.h>
#include "dsconvert_v1_to_v2.h"

#define MAX_INT64 ( (int64_t) ( ( ~ ((uint64_t) 0) ) >> 1 ) )
#define MAX_FILE_SIZE 4000000000
#define INVALID_REC_ID (-1)

using namespace std;
using namespace boost;
using namespace boost::multi_index;

/// Structure for extent type information
struct extent_type_info
{
    /// Name of the extent type
    const char *type_name;

    /// Field to do the merge on
    const char *field_name;

    /// Tells if the extent type is the primary
    /// extent type to perform the merge
    bool is_primary_extent;
};

/// Enter the info about extent-types that you would like to be part of the
/// merge. There should be exactly one primary_extent_type entry.
static extent_type_info extent_types[] = {

    {"trace::rpc", "record_id", true},
    {"trace::rpc::nfs3::access", "record_id_reply", false},
    {"trace::rpc::nfs3::commit", "record_id_reply", false},
    {"trace::rpc::nfs3::create", "record_id_reply", false},
    {"trace::rpc::nfs3::fsinfo", "record_id_reply", false},
    {"trace::rpc::nfs3::fsstat", "record_id_reply", false},
    {"trace::rpc::nfs3::getattr", "record_id_reply", false},
    {"trace::rpc::nfs3::link", "record_id_reply", false},
    {"trace::rpc::nfs3::lookup", "record_id_reply", false},
    {"trace::rpc::nfs3::pathconf", "record_id_reply", false},
    {"trace::rpc::nfs3::readdir", "record_id_reply", false},
    {"trace::rpc::nfs3::readdir::entries", "record_id_reply", false},
    {"trace::rpc::nfs3::readlink", "record_id_reply", false},
    {"trace::rpc::nfs3::readwrite", "record_id_reply", false},
    {"trace::rpc::nfs3::remove", "record_id_reply", false},
    {"trace::rpc::nfs3::rename", "record_id_reply", false},
    {"trace::rpc::nfs3::rwchecksum", "record_id_reply", false},
    {"trace::rpc::nfs3::setattr", "record_id_reply", false},
    {"trace::net::ip", "record_id", false},
    {"chronicle::stats", "time", false}
};

#define EXTENTTYPE_NUM sizeof extent_types / sizeof extent_types[0]

using namespace boost::filesystem;

bool skipType(const ExtentType::Ptr type) {
    return type->getName() == "DataSeries: ExtentIndex"
        || type->getName() == "DataSeries: XmlType"
        || (type->getName() == "Info::DSRepack"
                && type->getNamespace() == "ssd.hpl.hp.com");
}

bool strPrefix(const string & str, string & prefix)
{
    if(str.find(prefix) == 0)
        return true;
    else
        return false;
}
void * followerChronicleStats(void *arg);
void * followerCommon(void *arg);
void * followerNetIp(void *arg);
void * masterWorker(void *arg);
class DsConvert;

class ConvertExtentType {

    ExtentType::Ptr extype; //Source extent type associated with this thread
    ExtentType::Ptr out_extype; //Sink extent type associated with this thread
    std::string fieldName;          //Field to perform the merge on
    ExtentSeries outputseries;//(ExtentSeries::typeLoose);
    OutputModule *outmodule;
    DsConvert *dsConvert;
    pthread_t workerThread;

    public:


    ConvertExtentType(DsConvert *, const char* , const char* , bool);
    ~ConvertExtentType();
    void refreshOutput(void);
    void waitToFinish(void) {
        pthread_join(workerThread, NULL);
    }

    friend class DsConvert;
    friend void * followerChronicleStats(void *arg);
    friend void * followerCommon(void *arg);
    friend void * followerNetIp(void *arg);
    friend void * masterWorker(void *arg);
};


class DsConvert {

    pthread_cond_t masterDone, followerDone;
    pthread_mutex_t mtx;
    pthread_barrier_t initBarrier;
    int followerDoneCnt, followerThreadCnt;
    vector <std::string> *vec;  //list of source files
    DataSeriesSink *output;  //DataSink file
    //const char* extentType;
    std::string filePrefix;
    int64_t sharedVar;
    int64_t sharedTSVar; // shared timestamp variable, between master, followerCommon
    std::map<int64_t,int64_t> shared_rr_map; //stores a req-rec-id -> reply-rec-id mapping

    int extent_size, extentCount;
    ConvertExtentType **extConvertors;
    bool masterExited;
    ExtentTypeLibrary lib;  //for DataSink only
    unsigned int rotateCount;
    off64_t ROTATE_FILE_SIZE;
    off64_t curFileOffset;
    class WriteCallback;
    WriteCallback *writeCallback;

    public:

    friend class ConvertExtentType;
    friend void * followerChronicleStats(void *arg);
    friend void * followerCommon(void *arg);
    friend void * followerNetIp(void *arg);
    friend void * masterWorker(void *arg);

    DsConvert(vector <std::string> *);
    ~DsConvert();
    void setupPerExTypeConvertors(void);
    void waitForWorkers(void);
    void rotateSink(void);
    std::string newFilename(void);

};

void DsConvert::waitForWorkers(void)
{
    for(unsigned int i = 0; i < EXTENTTYPE_NUM; ++i) {
        extConvertors[i]->waitToFinish();
    }
}

class DsConvert::WriteCallback {
    off64_t *currOffset;

    public:

    WriteCallback(off64_t *offsetPtr) : currOffset(offsetPtr) { }
    void operator()(off64_t offset, Extent &e) {
        *currOffset = offset;
    }

    void reset() {
        *currOffset = 0;
    }
};

std::string DsConvert::newFilename(void)
{
    char num[6];
    sprintf(num, "%05u", rotateCount++);
    return filePrefix  + num + ".ds";
}

void DsConvert::rotateSink()
{
    for(int i = 0; i < extentCount; ++i) {
        if(extConvertors[i]->outmodule) {
            delete extConvertors[i]->outmodule;
        }
    }

    if(output) {
        output->flushPending();
        output->getStats().printText(std::cout);
        delete output;
    }

    int compression_mode =
        Extent::compression_algs[Extent::compress_mode_bz2].compress_flag;
    output = new DataSeriesSink(newFilename(), compression_mode, 9);
    output->setMaxBytesInProgress(8147483648);
    writeCallback->reset();
    output->setExtentWriteCallback(*writeCallback);

    for(int i = 0; i < extentCount; ++i) {
        extConvertors[i]->refreshOutput();
    }

    output->writeExtentLibrary(lib);
}

void DsConvert::setupPerExTypeConvertors()
{
    extentCount = EXTENTTYPE_NUM;
    extConvertors = new ConvertExtentType *[extentCount];
    pthread_barrier_init(&initBarrier, NULL, extentCount);
    for(int i = 0; i < extentCount; ++i) {
        extConvertors[i] = new ConvertExtentType(this, extent_types[i].field_name , extent_types[i].type_name, extent_types[i].is_primary_extent);
    }
    followerThreadCnt = extentCount - 1;
    followerDoneCnt = 0;
}




DsConvert::DsConvert(vector <string> *v)
{
    vec = v;

    //get the file prefix
    string some_filename = (path(vec->front()).filename()).string();
    size_t pos = 0;
    while ((pos = some_filename.find("_")) != std::string::npos) {
        filePrefix += some_filename.substr(0, pos) + "_";
        some_filename.erase(0, pos + 1);
    }
    boost::replace_all(filePrefix, "chronicle", "chronicle_v2");

    ROTATE_FILE_SIZE = MAX_FILE_SIZE;

    extentCount = 0;
    rotateCount = 0;

    //create a sink file and setup the extent library
    output = new DataSeriesSink(newFilename(), Extent::compress_mode_bz2, 9);
    output->setMaxBytesInProgress(8147483648);

    lib.registerTypePtr(EXTENT_CHRONICLE_NFS3_ACCESS);
    lib.registerTypePtr(EXTENT_CHRONICLE_NFS3_COMMIT);
    lib.registerTypePtr(EXTENT_CHRONICLE_NFS3_CREATE);
    lib.registerTypePtr(EXTENT_CHRONICLE_NFS3_FSINFO);
    lib.registerTypePtr(EXTENT_CHRONICLE_NFS3_FSSTAT);
    lib.registerTypePtr(EXTENT_CHRONICLE_NFS3_GETATTR);
    lib.registerTypePtr(EXTENT_CHRONICLE_IP);
    lib.registerTypePtr(EXTENT_CHRONICLE_NFS3_LINK);
    lib.registerTypePtr(EXTENT_CHRONICLE_NFS3_LOOKUP);
    lib.registerTypePtr(EXTENT_CHRONICLE_NFS3_PATHCONF);
    lib.registerTypePtr(EXTENT_CHRONICLE_NFS3_READWRITE);
    lib.registerTypePtr(EXTENT_CHRONICLE_NFS3_RWCHECKSUM);
    lib.registerTypePtr(EXTENT_CHRONICLE_NFS3_READDIR);
    lib.registerTypePtr(EXTENT_CHRONICLE_NFS3_READDIR_ENTRIES);
    lib.registerTypePtr(EXTENT_CHRONICLE_NFS3_READLINK);
    lib.registerTypePtr(EXTENT_CHRONICLE_NFS3_REMOVE);
    lib.registerTypePtr(EXTENT_CHRONICLE_NFS3_RENAME);
    lib.registerTypePtr(EXTENT_CHRONICLE_RPCCOMMON);
    lib.registerTypePtr(EXTENT_CHRONICLE_NFS3_SETATTR);
    lib.registerTypePtr(EXTENT_CHRONICLE_STATS);

    output->writeExtentLibrary(lib);
    output->flushPending();

    pthread_mutex_init(&mtx, NULL);
    pthread_cond_init (&masterDone, NULL);
    pthread_cond_init (&followerDone, NULL);
    sharedVar = 0;
    sharedTSVar = 0;
    masterExited = false;

    writeCallback = new WriteCallback(&curFileOffset);
    writeCallback->reset();
    output->setExtentWriteCallback(*writeCallback);

    // the analysis work in the paper shows that 64-96k is the
    // peak decompression rate, with a sacrifice of a slight
    // amount from the maximum compression ratio.  Compression
    // ratio flattens out somewhere around 512k, but if we are not
    // using bz2, then the goal is likely speed rather than
    // compression, so default to a smaller extent size.  For bz2,
    // we are going for maximal compression, which is flat past
    // 8MB.

    //   int extent_size = 64*1024;
    //    if (compress_modes & Extent::compress_bz2) {
    extent_size = 16*1024*1024;
    //    }
}

DsConvert::~DsConvert()
{
    delete writeCallback;

    for(int i = 0; i < extentCount; ++i) {
        delete extConvertors[i];
    }
    delete [] extConvertors;

    if(output) {
        output->flushPending();
        output->getStats().printText(std::cout);
        delete output;
    }

}

void ConvertExtentType::refreshOutput(void)
{
    outmodule = new OutputModule(*(dsConvert->output), outputseries, out_extype, dsConvert->extent_size);
}

ConvertExtentType::~ConvertExtentType()
{
    if(outmodule)
        delete outmodule;
}

ConvertExtentType::ConvertExtentType(DsConvert *dsConvert, const char* fieldName, const char *extentType, bool isMaster)
{

    this->dsConvert = dsConvert;
    this->fieldName = fieldName;
    string extentTypePrefix(extentType);

    //Create a dummy source just to call getLibrary on it. Is there a better way?
    DataSeriesSource some_file(dsConvert->vec->front());
    extype = some_file.getLibrary().getTypeByNamePtr(extentTypePrefix);
    INVARIANT(extype != NULL, boost::format("Couldn't find a type matching extent type %s") % extentTypePrefix);
    some_file.closefile();

    //get DataSink's output extent type
    out_extype = (dsConvert->lib).getTypeByNamePtr(extentTypePrefix);
    outputseries.setType(out_extype);
    outmodule = new OutputModule(*(dsConvert->output), outputseries, out_extype, dsConvert->extent_size);

    if(strcmp(fieldName, "time")) {
        if(isMaster) {
            pthread_create(&workerThread, NULL, &masterWorker, (void *)this);
        }
        else if(extentTypePrefix == "trace::net::ip") {
            pthread_create(&workerThread, NULL, &followerNetIp, (void *)this);
        }
        else {
            pthread_create(&workerThread, NULL, &followerCommon, (void *)this);
        }
    }
    else {
        pthread_create(&workerThread, NULL, &followerChronicleStats, (void *)this);
    }
}

void display_usage()
{
    std::cout << "dsconvert_v1_to_v2 <ds-file-list...>" << std::endl;
    cout<< "This tool converts NFSv3 dataseries traces from version 1.0 to version 2.0.\n"
        "The output files are populated in the current/working directory. The output \n"
        "filenames have prefix of the form: <chronicle_timestamp_V2> where timestamp \n"
        "is the same as the source file"<<endl;
}

int main(int argc, char *argv[])
{
    if(argc < 2) {
        display_usage();
        exit(EXIT_FAILURE);
    }

    std::vector<std::string> vec;
    for(int i = 1; i < argc; ++i) {
        if(is_regular_file(path(argv[i])))
            vec.push_back(argv[i]);
        else {
            display_usage();
            exit(EXIT_FAILURE);
        }
    }


    DsConvert convertDsFiles(&vec);
    convertDsFiles.setupPerExTypeConvertors();
    convertDsFiles.waitForWorkers();

    return 0;
}

void * followerChronicleStats(void *arg)
{
    ConvertExtentType *ti = (ConvertExtentType *)arg;
    DsConvert *sharedInfo = ti->dsConvert;

    SortedSourcePipe *sourcePipes = new SortedSourcePipe(ti->extype);

    for (vector<string>::iterator it(sharedInfo->vec->begin()), it_end(sharedInfo->vec->end()); it != it_end; it++) {
            sourcePipes->addSource((*it));
    }
    sourcePipes->startPrefetching();

    vector<GeneralField *> outfields;

    int primaryFieldNum = -1;
    for(unsigned i = 0; i < ti->out_extype->getNFields(); ++i) {
        outfields.push_back(GeneralField::create(NULL, ti->outputseries, ti->out_extype->getFieldName(i)));
        if(ti->out_extype->getFieldName(i) == ti->fieldName) {
            primaryFieldNum = i;
            cout << "Primary Field number for extent type "<< ti->out_extype->getName()<< " and field name " << ti->fieldName << ": "<<primaryFieldNum << endl;
        }
    }
    assert(primaryFieldNum != -1);//, boost::format("Couldn't find a field matching %s") % ti->fieldName);

    uint64_t output_row_count = 0;

    //Fetch the very first record from the source pipe...
    int64_t current_record_id = INVALID_REC_ID;
    if(sourcePipes->getFirstRecord()) {

        GeneralValue v(sourcePipes->infields[primaryFieldNum]);
        current_record_id = v.valInt64();
    }

    pthread_barrier_wait(&sharedInfo->initBarrier);
    pthread_mutex_lock(&sharedInfo->mtx);
    pthread_cond_wait (&sharedInfo->masterDone, &sharedInfo->mtx);
    pthread_mutex_unlock(&sharedInfo->mtx);

    while(current_record_id != INVALID_REC_ID) {

        GeneralValue v(sourcePipes->infields[primaryFieldNum]);

        pthread_mutex_lock(&sharedInfo->mtx);

        if(!sharedInfo->masterExited) {
            while(v.valInt64() > sharedInfo->sharedTSVar) {
                (sharedInfo->followerDoneCnt)++;
                if(sharedInfo->followerDoneCnt == sharedInfo->followerThreadCnt) {

                    pthread_cond_signal(&sharedInfo->followerDone);
                    sharedInfo->followerDoneCnt = 0;
                }

                pthread_cond_wait (&sharedInfo->masterDone, &sharedInfo->mtx);
            }
        }
        pthread_mutex_unlock(&sharedInfo->mtx);


        if(v.valInt64() <= sharedInfo->sharedTSVar) {
            //create room for new record in the sink file
            ti->outmodule->newRecord();
            ++output_row_count;
            for(unsigned int i=0; i< (sourcePipes->infields).size(); ++i) {
                outfields[i]->set(sourcePipes->infields[i]);
            }
            current_record_id = INVALID_REC_ID;

            //get next record from the source pipe
            if(sourcePipes->getNextRecord()) {
                GeneralValue v(sourcePipes->infields[primaryFieldNum]);
                current_record_id = v.valInt64();
            }
        }
    }


    pthread_mutex_lock(&sharedInfo->mtx);
    if(!sharedInfo->masterExited) {
        (sharedInfo->followerThreadCnt)--;
        if(sharedInfo->followerDoneCnt == sharedInfo->followerThreadCnt) {
            sharedInfo->followerDoneCnt = 0;
            pthread_cond_signal(&sharedInfo->followerDone);
        }
    }
    pthread_mutex_unlock(&sharedInfo->mtx);

    ti->outmodule->flushExtent();
    ti->outmodule->close();

    GeneralField::deleteFields(outfields);

    cout << "Extent type: "<< ti->extype->getName()<< ", Total input rows: " << sourcePipes->input_row_count << ", Total Output rows: " << output_row_count<< "\n";

    return 0;
}


void * followerCommon(void *arg)
{
    ConvertExtentType *ti = (ConvertExtentType *)arg;
    DsConvert *sharedInfo = ti->dsConvert;

    SortedSourcePipe *sourcePipes = new SortedSourcePipe(ti->extype);


    for (vector<string>::iterator it(sharedInfo->vec->begin()), it_end(sharedInfo->vec->end()); it != it_end; it++) {
            sourcePipes->addSource((*it));
    }
    sourcePipes->startPrefetching();

    vector<GeneralField *> outfields;

    int primaryFieldNum = -1;
    for(unsigned i = 0; i < ti->extype->getNFields(); ++i) {
        if(ti->extype->getFieldName(i) == ti->fieldName) {
            primaryFieldNum = i;
            cout << "Primary Field number for extent type "<< ti->extype->getName()<< " and field name " << ti->fieldName << ": "<<primaryFieldNum << endl;
        }
    }
    assert(primaryFieldNum != -1);//, boost::format("Couldn't find a field matching %s") % ti->fieldName);
    for(unsigned i = 0; i < ti->out_extype->getNFields(); ++i) {
        outfields.push_back(GeneralField::create(NULL, ti->outputseries, ti->out_extype->getFieldName(i)));
    }

    uint64_t output_row_count = 0;

    //Fetch the very first record from the source pipe...
    int64_t current_record_id = INVALID_REC_ID;
    if(sourcePipes->getFirstRecord()) {

        GeneralValue v(sourcePipes->infields[primaryFieldNum]);
        current_record_id = v.valInt64();
    }

    pthread_barrier_wait(&sharedInfo->initBarrier); //wait for all threads to reach here
    pthread_mutex_lock(&sharedInfo->mtx);
    pthread_cond_wait (&sharedInfo->masterDone, &sharedInfo->mtx);
    pthread_mutex_unlock(&sharedInfo->mtx);

    int64_t tmp_rec_id = INT64_MIN;
    while(current_record_id != INVALID_REC_ID) {

        GeneralValue v(sourcePipes->infields[primaryFieldNum]);

        pthread_mutex_lock(&sharedInfo->mtx);

        if(v.valInt64() < tmp_rec_id) {
            std::cerr << "\33[0;31m" << "Extent type: " << ti->extype->getName() << " Current_rec_id = "<< v.valInt64() << " Last Record id was = "<< tmp_rec_id<< "\33[0m"<<endl;
            assert(sharedInfo->sharedVar > tmp_rec_id);
         }
        tmp_rec_id = v.valInt64();


        if(!sharedInfo->masterExited) {
            while(v.valInt64() > sharedInfo->sharedVar) {
                (sharedInfo->followerDoneCnt)++;
                if(sharedInfo->followerDoneCnt == sharedInfo->followerThreadCnt) {

                    pthread_cond_signal(&sharedInfo->followerDone);
                    sharedInfo->followerDoneCnt = 0;

                }

                pthread_cond_wait (&sharedInfo->masterDone, &sharedInfo->mtx);
            }
        }
        pthread_mutex_unlock(&sharedInfo->mtx);


        if(v.valInt64() <= sharedInfo->sharedVar) {
            //create room for new record in the sink file
            ti->outmodule->newRecord();
            ++output_row_count;
            outfields[0]->set(sourcePipes->infields[1]);    // v2.record_id = v1.record_id_reply
            for(unsigned int i=1; i< outfields.size(); ++i) {
                outfields[i]->set(sourcePipes->infields[i + 1]);    //copy all other fields
            }
            current_record_id = INVALID_REC_ID;
            //get next record from the source pipe
            if(sourcePipes->getNextRecord()) {
                GeneralValue v(sourcePipes->infields[primaryFieldNum]);
                current_record_id = v.valInt64();
            }
        }
    }


    pthread_mutex_lock(&sharedInfo->mtx);
    if(!sharedInfo->masterExited) {
        (sharedInfo->followerThreadCnt)--;
        if(sharedInfo->followerDoneCnt == sharedInfo->followerThreadCnt) {
            sharedInfo->followerDoneCnt = 0;
            pthread_cond_signal(&sharedInfo->followerDone);
        }
    }
    pthread_mutex_unlock(&sharedInfo->mtx);

    ti->outmodule->flushExtent();
    ti->outmodule->close();

    GeneralField::deleteFields(outfields);

    cout << "Extent type: "<< ti->extype->getName()<< ", Total input rows: " << sourcePipes->input_row_count << ", Total Output rows: " << output_row_count<< "\n";

    return 0;
}



#define IP_TABLE_SIZE 40000

struct IpTable {

    struct ip_pkt
    {
        int64_t rec_id;
        mutable vector<GeneralValue> fields;

        ip_pkt(int64_t rId, unsigned int numFields)
            {

                rec_id = rId;
                for(unsigned int i=0;i<numFields;i++)
                    fields.push_back(GeneralValue());
            }
    };
    unsigned int srcNumFields, srcPrimaryFieldNum ,outNumFields, outPrimaryFieldNum;

    IpTable(unsigned int srcnum, unsigned int src_primary_field, unsigned int outnum, unsigned int out_primary_field):srcNumFields(srcnum),srcPrimaryFieldNum(src_primary_field), outNumFields(outnum),outPrimaryFieldNum(out_primary_field){
    }

    typedef multi_index_container <
    ip_pkt,
        indexed_by <
            ordered_non_unique<
                member<
                ip_pkt,int64_t,&ip_pkt::rec_id
                >
            >
        >
    > ip_pkt_container;

    typedef nth_index<ip_pkt_container,0>::type ordered_recid;

    ip_pkt_container pkts;

    void insert(vector<GeneralField *>& srcfields, map<int64_t,int64_t>& rr_map) {
            std::map<int64_t,int64_t>::iterator it;
            GeneralValue gv;
            it = rr_map.find((srcfields[srcPrimaryFieldNum]->val()).valInt64());
            if(it!=rr_map.end()) {
                gv.setInt64(it->second);
                if(it!=rr_map.begin())
                    rr_map.erase(it);   //if this is not the smallest record_id
                                        //so far, then erase it to make room
            }
            else {
                gv = srcfields[srcPrimaryFieldNum]->val();
                //rp.record_id = srcfields[15];
            }
            ip_pkt rp = ip_pkt(gv.valInt64(),outNumFields);
            if(srcNumFields == 12) {   //Version 1
                rp.fields[0] = srcfields[0];
                for(unsigned int i=1;i < (srcNumFields - 1);i++) {
                    rp.fields[i+2] = srcfields[i];
                }
            }
            else if(16 == srcNumFields) {  //Version 1.1
                for(unsigned int i=0;i < outNumFields;i++) {
                    //GeneralValue c = srcfields[i];
                    rp.fields[i] = srcfields[i];
                }
            }
            else {
                assert(0 && "Unexpected number of fields in trace::net::ip");
            }
            rp.fields[outPrimaryFieldNum] = gv;

            pair<ip_pkt_container::iterator,bool> ret = pkts.insert(rp);
            if(ret.second == false) {
                cout << "WARNING: Insert into ip_pkt_container failed!\n";
            }
    }

    inline bool is_full(void) {
        if(IP_TABLE_SIZE <= pkts.size())
            return true;
        else
            return false;
    }

    inline bool is_empty(void) {
        return (pkts.size())? false: true;
    }

    bool get_next(vector<GeneralField *>& destfields) {

        if(pkts.size() != 0) {

            ordered_recid &rx = pkts.get<0>();
            ordered_recid::iterator it = rx.begin();

            for(unsigned int i=0;i<outNumFields;i++)
                destfields[i]->set(it->fields[i]);

            rx.erase(it);

            return true;
        }
        else
            return false;
    }
};

void * followerNetIp(void *arg)
{
    ConvertExtentType *ti = (ConvertExtentType *)arg;
    DsConvert *sharedInfo = ti->dsConvert;

    SortedSourcePipe *sourcePipes = new SortedSourcePipe(ti->extype);


    for (vector<string>::iterator it(sharedInfo->vec->begin()), it_end(sharedInfo->vec->end()); it != it_end; it++) {
            sourcePipes->addSource((*it));
    }
    sourcePipes->startPrefetching();

    vector<GeneralField *> outfields;

    int srcPrimaryFieldNum = -1;
    for(unsigned i = 0; i < ti->extype->getNFields(); ++i) {
        if(ti->extype->getFieldName(i) == ti->fieldName) {
            srcPrimaryFieldNum = i;
            cout << "Primary Field number for extent type "<< ti->extype->getName()<< " and field name " << ti->fieldName << ": "<<srcPrimaryFieldNum << endl;
        }
    }
    assert(srcPrimaryFieldNum != -1);//, boost::format("Couldn't find a field matching %s") % ti->fieldName);

    int outPrimaryFieldNum = -1;
    for(unsigned i = 0; i < ti->out_extype->getNFields(); ++i) {
        outfields.push_back(GeneralField::create(NULL, ti->outputseries, ti->out_extype->getFieldName(i)));
        if(ti->out_extype->getFieldName(i) == ti->fieldName)
            outPrimaryFieldNum = i;
    }
    assert(outPrimaryFieldNum != -1);

    IpTable ipTable(ti->extype->getNFields(), srcPrimaryFieldNum, ti->out_extype->getNFields(), outPrimaryFieldNum);

//sleep(10);
    pthread_barrier_wait(&sharedInfo->initBarrier); //wait for rest of the threads to reach here
    uint64_t output_row_count = 0;
    pthread_mutex_lock(&sharedInfo->mtx);
    pthread_cond_wait (&sharedInfo->masterDone, &sharedInfo->mtx);
    pthread_mutex_unlock(&sharedInfo->mtx);
    //Fetch the very first record from the source pipe...
    int64_t current_record_id = INVALID_REC_ID;
    if(sourcePipes->getFirstRecord()) {

        ipTable.insert(sourcePipes->infields, sharedInfo->shared_rr_map);

        while(!ipTable.is_full()) {
            if(sourcePipes->getNextRecord()) {
                ipTable.insert(sourcePipes->infields, sharedInfo->shared_rr_map);
            }
            else
                break;
        }

        //create a new record in the output module and copy
        //the first record from the IpTable into it
        ti->outmodule->newRecord();
        ++output_row_count;
        ipTable.get_next(outfields);
        current_record_id = GeneralValue(outfields[outPrimaryFieldNum]).valInt64();
    }

    int64_t tmp_rec_id = INT64_MIN;
    while(current_record_id != INVALID_REC_ID) {

        GeneralValue v(outfields[outPrimaryFieldNum]);

        pthread_mutex_lock(&sharedInfo->mtx);

        if(v.valInt64() < tmp_rec_id) {
            std::cerr << "\33[0;31m" << "Extent type: " << ti->extype->getName() << " Current_rec_id = "<< v.valInt64() << " Last Record id was = "<< tmp_rec_id<< "\33[0m"<<endl;
            assert(sharedInfo->sharedVar < tmp_rec_id);
         }
        tmp_rec_id = v.valInt64();


        if(!sharedInfo->masterExited) {
            while(v.valInt64() > sharedInfo->sharedVar) {
                (sharedInfo->followerDoneCnt)++;
                if(sharedInfo->followerDoneCnt == sharedInfo->followerThreadCnt) {

                    pthread_cond_signal(&sharedInfo->followerDone);
                    sharedInfo->followerDoneCnt = 0;

                }

                pthread_cond_wait (&sharedInfo->masterDone, &sharedInfo->mtx);
            }
        }
        pthread_mutex_unlock(&sharedInfo->mtx);


        if(v.valInt64() <= sharedInfo->sharedVar) {
            current_record_id = INVALID_REC_ID;

            if(!ipTable.is_empty()) {

                //create room for new record in the sink file
                ti->outmodule->newRecord();
                ++output_row_count;
                if(ipTable.get_next(outfields)) {

                    GeneralValue v(outfields[outPrimaryFieldNum]);
                    current_record_id = v.valInt64();
                    while(!ipTable.is_full()) {
                        if(sourcePipes->getNextRecord())
                            ipTable.insert(sourcePipes->infields, sharedInfo->shared_rr_map);
                        else
                            break;
                    }
                }
            }
        }
    }

    pthread_mutex_lock(&sharedInfo->mtx);
    if(!sharedInfo->masterExited) {
        (sharedInfo->followerThreadCnt)--;
        if(sharedInfo->followerDoneCnt == sharedInfo->followerThreadCnt) {
            sharedInfo->followerDoneCnt = 0;
            pthread_cond_signal(&sharedInfo->followerDone);
        }
    }
    pthread_mutex_unlock(&sharedInfo->mtx);

    ti->outmodule->flushExtent();
    ti->outmodule->close();

    GeneralField::deleteFields(outfields);

    cout << "Extent type: "<< ti->extype->getName()<< ", Total input rows: " << sourcePipes->input_row_count << ", Total Output rows: " << output_row_count<< "\n";

    return 0;
}


void MurmurHash3_x64_64(const void* tuple, int len, uint32_t seed, void* out)
{
    uint64_t _out[2];
    MurmurHash3_x64_128(tuple,len,seed,_out);
    *(uint64_t*)out = _out[0];
}

//The window of consecutive records within which we hope to find the
//corresponding reply for a request rpc appearing at the start of the window.
uint64_t RPC_PAIR_TABLE_SIZE = 20000;

struct RpcPairs {

    struct rpc_pair
    {
        uint64_t    xid;
        int64_t rec_id;
        mutable GeneralValue request_at, reply_at, client, client_port, server,
        server_port, is_udp, rpc_program, program_version, program_proc,
        operation, transaction_id, call_header_offset, call_payload_length,
        reply_header_offset, reply_payload_length, record_id;

        mutable bool is_request_at, is_reply_at, is_call_header_offset,
        is_call_payload_length, is_reply_header_offset, is_reply_payload_length;

        rpc_pair(uint64_t xId, int64_t rId):
            xid(xId),rec_id(rId){
                is_request_at = is_reply_at = is_call_header_offset =
                is_call_payload_length = is_reply_header_offset =
                is_reply_payload_length = false;
            }
    };

    struct change_rec_id {
        change_rec_id(int64_t new_id):new_id(new_id){}

        void operator() (rpc_pair& rp) {
            rp.rec_id = new_id;
        }

        private:
        int64_t new_id;
    };

    typedef multi_index_container <
    rpc_pair,
        indexed_by <
            hashed_unique<
                member<
                rpc_pair,uint64_t,&rpc_pair::xid
                >
            >,

            ordered_unique<
                member<
                rpc_pair,int64_t,&rpc_pair::rec_id
                >
            >
        >
    > rpc_pair_container;

    typedef nth_index<rpc_pair_container,0>::type hashed_xid;
    typedef nth_index<rpc_pair_container,1>::type ordered_recid;

    rpc_pair_container pairs;


    void insert(vector<GeneralField *>& srcfields, map<int64_t,int64_t>& rr_map) {
        // Calculate the hash of {xid,src_ip,dest_ip} tuple
        int32_t hash_tuple[5];
        uint64_t hash_val;
        int32_t src_ip = GeneralValue(srcfields[1]).valInt32();
        int32_t dest_ip = GeneralValue(srcfields[3]).valInt32();
        bool is_request = GeneralValue(srcfields[6]).valBool();
        int32_t src_port = GeneralValue(srcfields[2]).valInt32();
        int32_t dest_port = GeneralValue(srcfields[4]).valInt32();
        if(is_request) {
            hash_tuple[0] = GeneralValue(srcfields[11]).valInt32();
            hash_tuple[1] = src_ip;
            hash_tuple[2] = dest_ip;
            hash_tuple[3] = src_port;
            hash_tuple[4] = dest_port;
        }
        else {
            hash_tuple[0] = GeneralValue(srcfields[11]).valInt32();
            hash_tuple[1] = dest_ip;
            hash_tuple[2] = src_ip;
            hash_tuple[3] = dest_port;
            hash_tuple[4] = src_port;
        }

        MurmurHash3_x64_64(hash_tuple, int(sizeof(hash_tuple)), (uint32_t)(src_ip & dest_ip), &hash_val);

        hashed_xid &hx = pairs.get<0>();
        hashed_xid::iterator it = hx.find(hash_val);
        if(it != hx.end()) {    //found a request matching this hash_val,
                                //which means current rpc is the reply
            if(is_request) {
                cout << "Warning: Detected a Hash collision while RPC req-reply matching!"<<endl;
                return;
            }
            else if(!it->is_reply_at) {
                rr_map[GeneralValue(it->record_id).valInt64()] = GeneralValue(srcfields[14]).valInt64();
                it->reply_at = srcfields[0];
                it->is_reply_at = true;
                it->reply_header_offset = srcfields[12];
                it->is_reply_header_offset = true;
                it->reply_payload_length = srcfields[13];
                it->is_reply_payload_length = true;
                it->record_id = srcfields[14];

                ordered_recid::iterator r_it = pairs.project<1>(it);
                ordered_recid &rx = pairs.get<1>();
                assert((rx.modify(r_it, change_rec_id(GeneralValue(srcfields[14]).valInt64()))) && "Updating record_id failed, this record_id already exists!!"); //Overwrite with reply record_id
            }
            else
                return;
        }
        else {
            rpc_pair rp = rpc_pair(hash_val,GeneralValue(srcfields[14]).valInt64());
            if(GeneralValue(srcfields[6]).valBool()){ //request
                rp.request_at = srcfields[0];
                rp.client = srcfields[1];
                rp.client_port = srcfields[2];
                rp.server = srcfields[3];
                rp.server_port = srcfields[4];

                rp.call_header_offset = srcfields[12];
                rp.call_payload_length = srcfields[13];
                rp.is_call_header_offset = true;
                rp.is_call_payload_length = true;
                rp.is_request_at = true;
                rp.record_id = srcfields[14];
            }
            else {  //reply (with no matching req found)
                rp.reply_at = srcfields[0];
                rp.client = srcfields[3];
                rp.client_port = srcfields[4];
                rp.server = srcfields[1];
                rp.server_port = srcfields[2];

                rp.reply_header_offset = srcfields[12];
                rp.reply_payload_length = srcfields[13];
                rp.is_reply_header_offset = true;
                rp.is_reply_payload_length = true;
                rp.is_reply_at = true;
                rp.record_id = srcfields[14];
            }
            //fields with same values in both req & reply
            rp.is_udp = srcfields[5];
            rp.rpc_program = srcfields[7];
            rp.program_version = srcfields[8];
            rp.program_proc = srcfields[9];
            rp.operation = srcfields[10];
            rp.transaction_id = srcfields[11];

            pair<rpc_pair_container::iterator,bool> ret = pairs.insert(rp);
            if(!ret.second)
                cout <<"Warning: Inserting RPC pair in rpc_pair_container failed.."<<endl;

        }
    }

    bool is_full(void) {
        if(RPC_PAIR_TABLE_SIZE <= pairs.size())
            return true;
        else
            return false;
    }

    bool get_next(vector<GeneralField *>& destfields) {
        if(pairs.size() != 0) {
            ordered_recid &rx = pairs.get<1>();
            ordered_recid::iterator it = rx.begin();

            if(it->is_request_at)
                destfields[0]->set(it->request_at);
            else
                destfields[0]->setNull();
            if(it->is_reply_at)
                destfields[1]->set(it->reply_at);
            else
                destfields[1]->setNull();
            destfields[2]->set(it->client);
            destfields[3]->set(it->client_port);
            destfields[4]->set(it->server);
            destfields[5]->set(it->server_port);
            destfields[6]->set(it->is_udp);
            destfields[7]->set(it->rpc_program);
            destfields[8]->set(it->program_version);
            destfields[9]->set(it->program_proc);
            destfields[10]->set(it->operation);
            destfields[11]->set(it->transaction_id);
            if(it->is_call_header_offset)
                destfields[12]->set(it->call_header_offset);
            else
                destfields[12]->setNull();
            if(it->is_call_payload_length)
                destfields[13]->set(it->call_payload_length);
            else
                destfields[13]->setNull();
            if(it->is_reply_header_offset)
                destfields[14]->set(it->reply_header_offset);
            else
                destfields[14]->setNull();
            if(it->is_reply_payload_length)
                destfields[15]->set(it->reply_payload_length);
            else
                destfields[15]->setNull();
            destfields[16]->set(it->record_id);

            rx.erase(it);

            return true;
        }
        else
            return false;
    }
};

void * masterWorker(void *arg)
{
    RpcPairs rpTable;
    ConvertExtentType *ti = (ConvertExtentType *)arg;
    DsConvert *sharedInfo = ti->dsConvert;
    uint64_t extent_num = 0;
    bool synced_with_others = false;


    SortedSourcePipe *sourcePipes = new SortedSourcePipe(ti->extype);


    for (vector<string>::iterator it(sharedInfo->vec->begin()), it_end(sharedInfo->vec->end()); it != it_end; it++) {
        sourcePipes->addSource((*it));
    }
    sourcePipes->startPrefetching();

    vector<GeneralField *> outfields;

    int primaryFieldNum = -1;
    for(unsigned i = 0; i < ti->extype->getNFields(); ++i) {
        if(ti->extype->getFieldName(i) == ti->fieldName) {
            primaryFieldNum = i;
            cout << "Primary Field number for extent type "<< ti->extype->getName()<< " and field name " << ti->fieldName << ": "<<primaryFieldNum << endl;
        }
    }
    INVARIANT(primaryFieldNum != -1, boost::format("Couldn't find a field matching %s") % ti->fieldName);
    for(unsigned i = 0; i < ti->out_extype->getNFields(); ++i) {
        outfields.push_back(GeneralField::create(NULL, ti->outputseries, ti->out_extype->getFieldName(i)));
    }

    uint64_t output_row_count = 0;

    //Fetch the very first record from the source pipe...
    //and insert in the RpcPairs table
    int64_t current_record_id = INVALID_REC_ID;
    if(sourcePipes->getFirstRecord()) {

        GeneralValue v(sourcePipes->infields[primaryFieldNum]);
        current_record_id = v.valInt64();

        //Insert records in the RpcPairs table until its full. The table's size
        //can be adjusted by changing RPC_PAIR_TABLE_SIZE. This value represents
        //the window of consecutive records within which we hope to find the
        //corresponding reply for a request rpc at the start of the window.
        while(!rpTable.is_full()) {
            if(sourcePipes->getNextRecord()) {
                rpTable.insert(sourcePipes->infields, sharedInfo->shared_rr_map);
            }
            else
                break;
        }
    }

    //create a new record in the output module and copy
    //the first req-reply pair from the RpcPairs table into it
    ti->outmodule->newRecord();
    ++output_row_count;
    rpTable.get_next(outfields);
    pthread_barrier_wait(&sharedInfo->initBarrier); //wait for rest of the threads to initialize before proceeding

    int64_t tmp_rec_id = INT64_MIN;;
    while(current_record_id != INVALID_REC_ID) {

        //Set the state shared between the master/follower threads
        GeneralValue v(outfields[16]); //record_id
        sharedInfo->sharedVar = v.valInt64();
        GeneralValue ts_req(outfields[0]); //trace::rpc request_at (nullable)
        GeneralValue ts_reply(outfields[1]); //trace::rpc reply_at (nullable))
        sharedInfo->sharedTSVar = (ts_reply.valInt64() > ts_req.valInt64())? ts_reply.valInt64() : ts_req.valInt64(); //pick whichever is greater
        if(sharedInfo->sharedVar < tmp_rec_id) {
            std::cerr << "\33[1;36mmbold" << "Extent type: " << ti->extype->getName() << " sharedVar = "<< sharedInfo->sharedVar << " Last Record id was = "<< tmp_rec_id<< "\33[0m"<<endl;
            assert(sharedInfo->sharedVar > tmp_rec_id);
        }
        tmp_rec_id = sharedInfo->sharedVar;

        //increment the output module
        ti->outmodule->newRecord();
        ++output_row_count;

        current_record_id = INVALID_REC_ID;

        if(rpTable.get_next(outfields)) {

            GeneralValue v(outfields[16]);
            current_record_id = v.valInt64();
            while(!rpTable.is_full()) {
                if(sourcePipes->getNextRecord())
                    rpTable.insert(sourcePipes->infields, sharedInfo->shared_rr_map);
                else
                    break;
            }
        }

        if(sharedInfo->curFileOffset > sharedInfo->ROTATE_FILE_SIZE) {
            cout<< "\n--------------------\nRotating sinks....\n";
            pthread_mutex_lock(&sharedInfo->mtx);
            pthread_cond_broadcast(&sharedInfo->masterDone);
            pthread_cond_wait(&sharedInfo->followerDone, &sharedInfo->mtx);
            pthread_mutex_unlock(&sharedInfo->mtx);
            sharedInfo->rotateSink();

            ti->outmodule->newRecord(); ++output_row_count;
            if(rpTable.get_next(outfields)) {
                GeneralValue v(outfields[16]);
                current_record_id = v.valInt64();
                while(!rpTable.is_full()) {
                    if(sourcePipes->getNextRecord())
                        rpTable.insert(sourcePipes->infields, sharedInfo->shared_rr_map);
                    else
                        break;
                }
            }
        }

        if((ti->outmodule->curExtentSize() > (0.8 * ti->outmodule->getTargetExtentSize())) && !synced_with_others) {
            cout << boost::format("Processing extent #%d of size %d nearing the target size of %d\n") % extent_num++ % ti->outmodule->curExtentSize() % ti->outmodule->getTargetExtentSize();

            pthread_mutex_lock(&sharedInfo->mtx);
            pthread_cond_broadcast(&sharedInfo->masterDone);

            pthread_cond_wait(&sharedInfo->followerDone, &sharedInfo->mtx);
            pthread_mutex_unlock(&sharedInfo->mtx);
            synced_with_others = true;
        }
        if(ti->outmodule->curExtentSize() < (0.8 * ti->outmodule->getTargetExtentSize()))
            synced_with_others = false;

    }

    pthread_mutex_lock(&sharedInfo->mtx);
    sharedInfo->masterExited = true;
    sharedInfo->sharedTSVar = MAX_INT64;
    sharedInfo->sharedVar = MAX_INT64;
    pthread_cond_broadcast(&sharedInfo->masterDone);
    pthread_mutex_unlock(&sharedInfo->mtx);

    ti->outmodule->flushExtent();
    ti->outmodule->close();

    GeneralField::deleteFields(outfields);

    cout << "Extent type: "<< ti->extype->getName()<< ", Total input rows: " << sourcePipes->input_row_count << ", Total Output rows: " << output_row_count<< "\n";

    return 0;
}

