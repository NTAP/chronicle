#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <getopt.h>

#include <cstdint>
#include <iostream>
#include <cstdlib>
#include <vector>

#include <boost/filesystem.hpp>
#include <pthread.h>
#include <time.h>
#include <limits.h>
#include "dsconvert_v2_to_v1.h"

#define MAX_INT64 ( (int64_t) ( ( ~ ((uint64_t) 0) ) >> 1 ) )
#define MAX_FILE_SIZE 4000000000
#define INVALID_REC_ID (-1)
#define V1_REQ_REC_ID(X) (X*2)
#define V1_REPLY_REC_ID(X) ((X*2) + 1)

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
    {"trace::rpc::nfs3::access", "record_id", false},
    {"trace::rpc::nfs3::commit", "record_id", false},
    {"trace::rpc::nfs3::create", "record_id", false},
    {"trace::rpc::nfs3::fsinfo", "record_id", false},
    {"trace::rpc::nfs3::fsstat", "record_id", false},
    {"trace::rpc::nfs3::getattr", "record_id", false},
    {"trace::rpc::nfs3::link", "record_id", false},
    {"trace::rpc::nfs3::lookup", "record_id", false},
    {"trace::rpc::nfs3::pathconf", "record_id", false},
    {"trace::rpc::nfs3::readdir", "record_id", false},
    {"trace::rpc::nfs3::readdir::entries", "record_id", false},
    {"trace::rpc::nfs3::readlink", "record_id", false},
    {"trace::rpc::nfs3::readwrite", "record_id", false},
    {"trace::rpc::nfs3::remove", "record_id", false},
    {"trace::rpc::nfs3::rename", "record_id", false},
    {"trace::rpc::nfs3::rwchecksum", "record_id", false},
    {"trace::rpc::nfs3::setattr", "record_id", false},
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
void * masterWorker(void *arg);
class DsConvert;

class ConvertExtentType {

    ExtentType::Ptr extype;	//Source extent type associated with this thread
    ExtentType::Ptr out_extype; //Sink extent type associated with this thread
    std::string fieldName;			//Field to perform the merge on
    ExtentSeries outputseries;//(ExtentSeries::typeLoose);
    OutputModule *outmodule;
    DsConvert *dsMerge;
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
    friend void * masterWorker(void *arg);
};


class DsConvert {

    pthread_cond_t masterDone, followerDone;
    pthread_mutex_t mtx;
    int followerDoneCnt, followerThreadCnt;
    vector <std::string> *vec;  //list of source files
    DataSeriesSink *output;  //DataSink file
    //const char* extentType;
    std::string filePrefix;
    int64_t sharedVar;
	int64_t sharedTSVar; // shared timestamp variable, between master, followerCommon

    int extent_size, extentCount;
    ConvertExtentType **mergeWorkers;
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
    friend void * masterWorker(void *arg);

    DsConvert(vector <std::string> *);
    ~DsConvert();
    void setupPerExtentTypeWorkers(void);
    void waitForWorkers(void);
    void rotateSink(void);
    std::string newFilename(void);

};

void DsConvert::waitForWorkers(void)
{
    for(unsigned int i = 0; i < EXTENTTYPE_NUM; ++i) {
        mergeWorkers[i]->waitToFinish();
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
    return filePrefix + "_" + num + ".ds";
}

void DsConvert::rotateSink()
{
    for(int i = 0; i < extentCount; ++i) {
        if(mergeWorkers[i]->outmodule) {
          //  mergeWorkers[i]->outmodule->flushExtent();
          //  mergeWorkers[i]->outmodule->close();
            delete mergeWorkers[i]->outmodule;
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
        mergeWorkers[i]->refreshOutput();
    }

    output->writeExtentLibrary(lib);
}

void DsConvert::setupPerExtentTypeWorkers()
{
    extentCount = EXTENTTYPE_NUM;
    mergeWorkers = new ConvertExtentType *[extentCount];
    for(int i = 0; i < extentCount; ++i) {
        mergeWorkers[i] = new ConvertExtentType(this, extent_types[i].field_name , extent_types[i].type_name, extent_types[i].is_primary_extent);
    }
    followerThreadCnt = extentCount - 1;
    followerDoneCnt = 0;
}




DsConvert::DsConvert(vector <string> *v)
{
    vec = v;

    //get the file prefix
    string some_filename = vec->front();
    size_t pos = 0;
    while ((pos = some_filename.find("_")) != std::string::npos) {
        filePrefix += some_filename.substr(0, pos);
        some_filename.erase(0, pos + 1);
    }

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
        delete mergeWorkers[i];
    }
    delete [] mergeWorkers;

    if(output) {
        output->flushPending();
        output->getStats().printText(std::cout);
        delete output;
    }

}

void ConvertExtentType::refreshOutput(void)
{
    outmodule = new OutputModule(*(dsMerge->output), outputseries, out_extype, dsMerge->extent_size);
}

ConvertExtentType::~ConvertExtentType()
{
    if(outmodule)
        delete outmodule;
}

ConvertExtentType::ConvertExtentType(DsConvert *dsMerge, const char* fieldName, const char *extentType, bool isMaster)
{

    this->dsMerge = dsMerge;
    this->fieldName = fieldName;
    string extentTypePrefix(extentType);

    //Create a dummy source just to call getLibrary on it. Is there a better way?
    DataSeriesSource some_file(dsMerge->vec->front());
    extype = some_file.getLibrary().getTypeByNamePtr(extentTypePrefix);
    INVARIANT(extype != NULL, boost::format("Couldn't find a type matching extent type %s") % extentTypePrefix);
    some_file.closefile();

    //get DataSink's output extent type
    out_extype = (dsMerge->lib).getTypeByNamePtr(extentTypePrefix);
    outputseries.setType(out_extype);
    outmodule = new OutputModule(*(dsMerge->output), outputseries, out_extype, dsMerge->extent_size);

	if(strcmp(fieldName, "time")) {
    if(isMaster) {
        pthread_create(&workerThread, NULL, &masterWorker, (void *)this);
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
    std::cout << "dsconvert <ds-file-list...>" << std::endl;
}

int main(int argc, char *argv[])
{
    if(argc < 2) {
        display_usage();
        exit(EXIT_FAILURE);
    }

    std::vector<std::string> vec;
    for(int i = 1; i < argc; ++i) {
        vec.push_back(argv[i]);
    }


    DsConvert mergeDsFiles(&vec);
    mergeDsFiles.setupPerExtentTypeWorkers();
    mergeDsFiles.waitForWorkers();

    return 0;
}

void * followerChronicleStats(void *arg)
{
    ConvertExtentType *ti = (ConvertExtentType *)arg;
    DsConvert *sharedInfo = ti->dsMerge;

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
    DsConvert *sharedInfo = ti->dsMerge;

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


    pthread_mutex_lock(&sharedInfo->mtx);
    pthread_cond_wait (&sharedInfo->masterDone, &sharedInfo->mtx);
    pthread_mutex_unlock(&sharedInfo->mtx);

    int64_t tmp_rec_id = INT64_MIN;
    while(current_record_id != INVALID_REC_ID) {

        GeneralValue v(sourcePipes->infields[primaryFieldNum]);

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
            //create room for new record in the sink file
            ti->outmodule->newRecord();
            ++output_row_count;
            if("trace::net::ip" == ti->extype->getName()) { //is extent type == trace::net::ip

                if(2049 == (sourcePipes->infields[7]->val()).valInt32()) { //is request packet?
                    GeneralValue req_recid;
                    req_recid.setInt64(V1_REQ_REC_ID(current_record_id));
                    outfields[15]->set(req_recid);
                }
                else {  //is reply packet?
                    GeneralValue reply_recid;
                    reply_recid.setInt64(V1_REPLY_REC_ID(current_record_id));
                    outfields[15]->set(reply_recid);
                }

                for(unsigned i=0; i<=14 ; ++i) {
                    outfields[i]->set(sourcePipes->infields[i]);
                }
            }
            else {
                GeneralValue req_recid, reply_recid;
                req_recid.setInt64(V1_REQ_REC_ID(current_record_id));
                reply_recid.setInt64(V1_REPLY_REC_ID(current_record_id));
                outfields[0]->set(req_recid);
                outfields[1]->set(reply_recid);
                for(unsigned int i=1; i< (sourcePipes->infields).size(); ++i) {
                    outfields[i + 1]->set(sourcePipes->infields[i]);
                }
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

void * masterWorker(void *arg)
{
    ConvertExtentType *ti = (ConvertExtentType *)arg;
    DsConvert *sharedInfo = ti->dsMerge;
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
    int64_t current_record_id = INVALID_REC_ID;
    if(sourcePipes->getFirstRecord()) {

        GeneralValue v(sourcePipes->infields[primaryFieldNum]);
        current_record_id = v.valInt64();
    }


    sleep(4);

    int64_t tmp_rec_id = INT64_MIN;;
    while(current_record_id != INVALID_REC_ID) {

        GeneralValue v(sourcePipes->infields[primaryFieldNum]);
        sharedInfo->sharedVar = v.valInt64();
		GeneralValue ts_req(sourcePipes->infields[0]); //trace::rpc request_at (nullable)
        GeneralValue ts_reply(sourcePipes->infields[1]); //trace::rpc reply_at (nullable))
		sharedInfo->sharedTSVar = (ts_reply.valInt64() > ts_req.valInt64())? ts_reply.valInt64() : ts_req.valInt64(); //pick whichever is greater

        if(sharedInfo->sharedVar < tmp_rec_id) {
            std::cerr << "\33[0;31m" << "Extent type: " << ti->extype->getName() << " sharedVar = "<< sharedInfo->sharedVar << " Last Record id was = "<< tmp_rec_id<< "\33[0m"<<endl;
            assert(sharedInfo->sharedVar < tmp_rec_id);
        }
        tmp_rec_id = sharedInfo->sharedVar;

        ti->outmodule->newRecord();
        ++output_row_count;

        if(sourcePipes->infields[0] != 0) { //trace::rpc request_at != null
            GeneralValue is_request;
            GeneralValue reply_recid, req_recid;

            is_request.setBool(true);
            outfields[6]->set(is_request) ;  //set trace::rpc v1.is_request = true
            outfields[0]->set(sourcePipes->infields[0]);  //trace::rpc v1.packet_at = v2.request_at
            for(unsigned i = 1; i <= 5; ++i) {
                outfields[i]->set(sourcePipes->infields[i + 1]);
            }
            for(unsigned i = 7; i <= 13; ++i) {
                outfields[i]->set(sourcePipes->infields[i]);
            }
            req_recid.setInt64(V1_REQ_REC_ID(current_record_id));
            outfields[14]->set(req_recid);

            //Create a new output record for the reply
            ti->outmodule->newRecord();
            ++output_row_count;

            reply_recid.setInt64(V1_REPLY_REC_ID(current_record_id));
            outfields[14]->set(reply_recid);
            is_request.setBool(false);
            outfields[6]->set(is_request);
            outfields[0]->set(sourcePipes->infields[1]);  //trace::rpc v1.packet_at = v2.reply_at
            outfields[1]->set(sourcePipes->infields[4]);  //trace::rpc v1.source = v2.server
            outfields[2]->set(sourcePipes->infields[5]);  //trace::rpc v1.source_port = v2.server_port
            outfields[3]->set(sourcePipes->infields[2]);  //trace::rpc v1.dest = v2.client
            outfields[4]->set(sourcePipes->infields[3]);  //trace::rpc v1.dest_port = v2.client_port
            outfields[5]->set(sourcePipes->infields[6]);
            for(unsigned i = 7; i <= 11; ++i) {
                outfields[i]->set(sourcePipes->infields[i]);
            }
            outfields[12]->set(sourcePipes->infields[14]);
            outfields[13]->set(sourcePipes->infields[15]);
        }
        current_record_id = INVALID_REC_ID;

        if(sourcePipes->getNextRecord()) {
            GeneralValue v(sourcePipes->infields[primaryFieldNum]);
            current_record_id = v.valInt64();
        }

        if(sharedInfo->curFileOffset > sharedInfo->ROTATE_FILE_SIZE) {
            cout<< "\n--------------------\nRotating sinks....\n";
			pthread_mutex_lock(&sharedInfo->mtx);
			pthread_cond_broadcast(&sharedInfo->masterDone);
			pthread_cond_wait(&sharedInfo->followerDone, &sharedInfo->mtx);
			pthread_mutex_unlock(&sharedInfo->mtx);
            sharedInfo->rotateSink();
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
