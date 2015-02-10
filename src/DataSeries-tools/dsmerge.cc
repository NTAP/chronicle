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
#include "dsmerge.h"
#define MAX_INT64 ( (int64_t) ( ( ~ ((uint64_t) 0) ) >> 1 ) )

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
void * slaveTSWorker(void *arg);
void * slaveWorker(void *arg);
void * masterWorker(void *arg);
class DSMerge;

class TypeExtentMerge {

    ExtentType::Ptr extype;	//Extent type associated with this thread
    std::string fieldName;			//Field to perform the merge on
    ExtentSeries outputseries;//(ExtentSeries::typeLoose);
    OutputModule *outmodule;
    DSMerge *dsMerge;
    pthread_t workerThread;

    public:


    TypeExtentMerge(DSMerge *, const char* , const char* , bool);
    ~TypeExtentMerge();
    void refreshOutput(void);
    void waitToFinish(void) {
        pthread_join(workerThread, NULL);
    }

    friend class DSMerge;
	friend void * slaveTSWorker(void *arg);
    friend void * slaveWorker(void *arg);
    friend void * masterWorker(void *arg);
};


class DSMerge {

    pthread_cond_t masterDone, slaveDone;
    pthread_mutex_t mtx;
    int numPipelines, slaveDoneCnt, slaveThreadCnt;
    vector <path> *vec;
    DataSeriesSink *output;
    //const char* extentType;
    std::string filePrefix, outDir;
    int64_t sharedVar;
	int64_t sharedTSVar; // shared timestamp variable, between master, slave

    int extent_size, extentCount;
    TypeExtentMerge **mergeWorkers;
    bool masterExited;
    ExtentTypeLibrary lib;
    unsigned int rotateCount;
    off64_t ROTATE_FILE_SIZE;
    off64_t curFileOffset;
    class WriteCallback;
    WriteCallback *writeCallback;

    public:

    friend class TypeExtentMerge;
	friend void * slaveTSWorker(void *arg);
    friend void * slaveWorker(void *arg);
    friend void * masterWorker(void *arg);

    DSMerge(int, vector <path> *, string, string , uint64_t);
    ~DSMerge();
    void setupPerExtentTypeWorkers(void);
    void waitForWorkers(void);
    void rotateSink(void);
    std::string newFilename(void);

};

void DSMerge::waitForWorkers(void)
{
    for(unsigned int i = 0; i < EXTENTTYPE_NUM; ++i) {
        mergeWorkers[i]->waitToFinish();
    }
}

class DSMerge::WriteCallback {
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

std::string DSMerge::newFilename(void)
{
    char num[6];
    sprintf(num, "%05u", rotateCount++);
    return outDir + filePrefix + "_" + num + ".ds";
}

void DSMerge::rotateSink()
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

void DSMerge::setupPerExtentTypeWorkers()
{
    extentCount = EXTENTTYPE_NUM;
    mergeWorkers = new TypeExtentMerge *[extentCount];
    for(int i = 0; i < extentCount; ++i) {
        mergeWorkers[i] = new TypeExtentMerge(this, extent_types[i].field_name , extent_types[i].type_name, extent_types[i].is_primary_extent);
    }
    slaveThreadCnt = extentCount - 1;
    slaveDoneCnt = 0;
}




DSMerge::DSMerge(int n_pipelines, vector <path> *v, string file_prefix, string output_dir = "", uint64_t chunk_size = 4000000000)
{
    numPipelines = n_pipelines;
    vec = v;
    filePrefix = file_prefix;
	ROTATE_FILE_SIZE = chunk_size;
	outDir = output_dir;
	if(!outDir.empty()) {
		if(*outDir.rbegin() != '/') {
			outDir.append("/");
		}
	}

    DataSeriesSource f((vec->front()).generic_string());
    extentCount = 0;
    rotateCount = 0;
    output = new DataSeriesSink(newFilename(), Extent::compress_mode_bz2, 9);
	output->setMaxBytesInProgress(8147483648);

    for(ExtentTypeLibrary::NameToType::iterator j = f.getLibrary().name_to_type.begin();
            j != f.getLibrary().name_to_type.end(); ++j) {

        if (skipType(j->second)) {
            continue;
        }

        const ExtentType::Ptr tmp = lib.getTypeByNamePtr(j->first, true);
        INVARIANT(tmp == NULL || tmp == j->second,
                boost::format("XML types for type '%s' differ between files") % j->first);
        if(!tmp) {
            ++extentCount;
            lib.registerTypePtr(j->second->getXmlDescriptionString());
            cout << "Registering type of name " << j->first << endl;
        }
    }
    output->writeExtentLibrary(lib);
    output->flushPending();

    pthread_mutex_init(&mtx, NULL);
    pthread_cond_init (&masterDone, NULL);
    pthread_cond_init (&slaveDone, NULL);
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

DSMerge::~DSMerge()
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

void TypeExtentMerge::refreshOutput(void)
{
    outmodule = new OutputModule(*(dsMerge->output), outputseries, extype, dsMerge->extent_size);
}

TypeExtentMerge::~TypeExtentMerge()
{
    if(outmodule)
        delete outmodule;


}

TypeExtentMerge::TypeExtentMerge(DSMerge *dsMerge, const char* fieldName, const char *extentType, bool isMaster)
{

    this->dsMerge = dsMerge;
    this->fieldName = fieldName;
    string extentTypePrefix(extentType);

    //Create a dummy source just to call getLibrary on it. Is there a better way?
    DataSeriesSource some_file((dsMerge->vec->front()).generic_string());
    extype = some_file.getLibrary().getTypeByNamePtr(extentTypePrefix);
    INVARIANT(extype != NULL, boost::format("Couldn't find a type matching extent type %s") % extentTypePrefix);
    some_file.closefile();

    outputseries.setType(extype);
    outmodule = new OutputModule(*(dsMerge->output), outputseries, extype, dsMerge->extent_size);

	if(strcmp(fieldName, "time")) {
    if(isMaster) {
        pthread_create(&workerThread, NULL, &masterWorker, (void *)this);
    }
    else {
        pthread_create(&workerThread, NULL, &slaveWorker, (void *)this);
    }
	}
	else {
		pthread_create(&workerThread, NULL, &slaveTSWorker, (void *)this);
	}
}

void display_usage()
{
    std::cout << "dsmerge -i <input_directory> -p <file_prefix> -n <number_of_pipelines> [-o <output_directory> -s <chunk_size_in_MB>]" << std::endl;
}

    int
main(int argc, char *argv[])
{
    if(argc < 4) {
        display_usage();
        exit(EXIT_FAILURE);
    }

    path p;
    std::string dPath, filePrefix, oDir;

    int c, numPipelines = 0;
	uint64_t chunkSize = 4000000000;
    opterr = 0;

    while ((c = getopt (argc, argv, "i:o:p:n:s:h?")) != -1) {
        switch(c) {
            case 'i':
                dPath = optarg;
                p = optarg;
                break;

			case 'o':
				oDir = optarg;
				break;

            case 'p':
                filePrefix = optarg;
                break;

            case 'n':
                numPipelines = atoi(optarg);
                break;

			case 's':
				chunkSize = (1024 * 1024 * atoi(optarg));
				break;

            case '?':
            case 'h':
            default:
                display_usage();
                exit(EXIT_FAILURE);
        }

    }

    std::cout << "\ndirPath = " << dPath << "\nfilePrefix = " << filePrefix << "\nnumPipelines = " << numPipelines << std::endl;


    std::vector<path> vec;  //vector to store filenames
    try {
        if(exists(p)) {
            if (is_regular_file(p)) {
                cout << "\n Please specify a proper directory path\n\n";
                display_usage();
                exit(1);
            }

            else if(is_directory(p)) {

                copy(directory_iterator(p), directory_iterator(), back_inserter(vec));

                sort(vec.begin(), vec.end()); // sort the filenames

                for (vector<path>::iterator it(vec.begin()) ; it != vec.end(); ) {
                    if(!strPrefix((*it).filename().generic_string(), filePrefix))
                        vec.erase(it);      // remove the filenames which don't match the prefix
                    else
                        it++;
                }

                for (vector<path>::const_iterator it(vec.begin()), it_end(vec.end()); it != it_end; ++it) {
                    cout << "   " << (*it).filename() << '\n';
                }
            }
            else
                cout << p << " exists, but is neither a regular file nor a directory\n";
        }
        else
            cout << p << " does not exist\n";
    }
    catch (const filesystem_error& ex) {
        cout << ex.what() << '\n';
    }

    DSMerge mergeDsFiles(numPipelines, &vec, filePrefix, oDir, chunkSize);
    mergeDsFiles.setupPerExtentTypeWorkers();
    mergeDsFiles.waitForWorkers();

    return 0;
}

void * slaveTSWorker(void *arg)
{
    TypeExtentMerge *ti = (TypeExtentMerge *)arg;
    DSMerge *sharedInfo = ti->dsMerge;

    SortedSourcePipe **sourcePipes = new SortedSourcePipe* [sharedInfo->numPipelines];

    for(int i = 0; i < sharedInfo->numPipelines; ++i) {

        string subStr = sharedInfo->filePrefix + "_p" + boost::lexical_cast<std::string>(i) + "_";
        //source[i] = new TypeIndexModule(ti->extype->getName());
        sourcePipes[i] = new SortedSourcePipe(ti->extype);
        //        pipeline[i] = new SequenceModule(source[i]);

        for (vector<path>::iterator it(sharedInfo->vec->begin()), it_end(sharedInfo->vec->end()); it != it_end; it++) {
            if((*it).filename().generic_string().find(subStr) != string::npos)
                sourcePipes[i]->addSource((*it).generic_string());
        }
        sourcePipes[i]->startPrefetching();
    }

    vector<GeneralField *> outfields;

    int primaryFieldNum = -1;
    for(unsigned i = 0; i < ti->extype->getNFields(); ++i) {
        outfields.push_back(GeneralField::create(NULL, ti->outputseries, ti->extype->getFieldName(i)));
        if(ti->extype->getFieldName(i) == ti->fieldName) {
            primaryFieldNum = i;
            cout << "Primary Field number for extent type "<< ti->extype->getName()<< " and field name " << ti->fieldName << ": "<<primaryFieldNum << endl;
        }
    }
    assert(primaryFieldNum != -1);//, boost::format("Couldn't find a field matching %s") % ti->fieldName);

    uint64_t output_row_count = 0;

    std::map<int64_t, int> map_;
    for(int i = 0; i < sharedInfo->numPipelines ; ++i) {

        if(sourcePipes[i]->getFirstRecord()) {

            GeneralValue v(sourcePipes[i]->infields[primaryFieldNum]);
            int64_t record_id = v.valInt64();
            map_[record_id] = i;
        }
    }


    pthread_mutex_lock(&sharedInfo->mtx);
    pthread_cond_wait (&sharedInfo->masterDone, &sharedInfo->mtx);
    pthread_mutex_unlock(&sharedInfo->mtx);

    for( std::map<int64_t, int>::iterator iter = map_.begin();iter != map_.end(); ) {

        int index = iter->second;
        GeneralValue v(sourcePipes[index]->infields[primaryFieldNum]);


        pthread_mutex_lock(&sharedInfo->mtx);

        if(!sharedInfo->masterExited) {
            while(v.valInt64() > sharedInfo->sharedTSVar) {
                (sharedInfo->slaveDoneCnt)++;
                if(sharedInfo->slaveDoneCnt == sharedInfo->slaveThreadCnt) {

                    pthread_cond_signal(&sharedInfo->slaveDone);
                    sharedInfo->slaveDoneCnt = 0;
                }

                pthread_cond_wait (&sharedInfo->masterDone, &sharedInfo->mtx);
            }
        }
/*        else {
            cout << endl << endl << "######## Master has exited ... sharedTSVar:"<< sharedInfo->sharedTSVar << " Current-ip-TS:" <<v.valInt64() << endl;
        }
*/
        pthread_mutex_unlock(&sharedInfo->mtx);


        if(v.valInt64() <= sharedInfo->sharedTSVar) {
            ti->outmodule->newRecord();
            ++output_row_count;

            for(unsigned int i=0; i< (sourcePipes[index]->infields).size(); ++i) {
                outfields[i]->set(sourcePipes[index]->infields[i]);
            }

            map_.erase(iter);

            if(sourcePipes[index]->getNextRecord()) {
                GeneralValue v(sourcePipes[index]->infields[primaryFieldNum]);
                int64_t record_id = v.valInt64();
                map_[record_id] = index;
            }
        }

        iter = map_.begin();

    }


    pthread_mutex_lock(&sharedInfo->mtx);
    if(!sharedInfo->masterExited) {
        (sharedInfo->slaveThreadCnt)--;
        if(sharedInfo->slaveDoneCnt == sharedInfo->slaveThreadCnt) {
            sharedInfo->slaveDoneCnt = 0;
            pthread_cond_signal(&sharedInfo->slaveDone);
        }
    }
    //cout << "extype: "<< ti->extype->getName()<< "slaveDoneCnt: "<< sharedInfo->slaveDoneCnt<< endl;
    //cout << "extype: "<< ti->extype->getName()<< "slaveThreadCnt: "<< sharedInfo->slaveThreadCnt<< endl;
    pthread_mutex_unlock(&sharedInfo->mtx);

    ti->outmodule->flushExtent();
    ti->outmodule->close();

    GeneralField::deleteFields(outfields);

    uint64_t total_input_rows = 0;
    for(int i = 0; i < sharedInfo->numPipelines ; ++i) {
        //cout << "Source " << i<< ": "<< sourcePipes[i]->input_row_count << "\n";
        total_input_rows += sourcePipes[i]->input_row_count;
    }
    cout << "Extent type: "<< ti->extype->getName()<< ", Total input rows: " << total_input_rows << ", Total Output rows: " << output_row_count<< "\n";

	return 0;
}


void * slaveWorker(void *arg)
{
    TypeExtentMerge *ti = (TypeExtentMerge *)arg;
    DSMerge *sharedInfo = ti->dsMerge;

    SortedSourcePipe **sourcePipes = new SortedSourcePipe* [sharedInfo->numPipelines];

    for(int i = 0; i < sharedInfo->numPipelines; ++i) {

        string subStr = sharedInfo->filePrefix + "_p" + boost::lexical_cast<std::string>(i) + "_";
        //source[i] = new TypeIndexModule(ti->extype->getName());
        sourcePipes[i] = new SortedSourcePipe(ti->extype);
        //        pipeline[i] = new SequenceModule(source[i]);

        for (vector<path>::iterator it(sharedInfo->vec->begin()), it_end(sharedInfo->vec->end()); it != it_end; it++) {
            if((*it).filename().generic_string().find(subStr) != string::npos)
                sourcePipes[i]->addSource((*it).generic_string());
        }
        sourcePipes[i]->startPrefetching();

    }

    vector<GeneralField *> outfields;

    int primaryFieldNum = -1;
    for(unsigned i = 0; i < ti->extype->getNFields(); ++i) {
        outfields.push_back(GeneralField::create(NULL, ti->outputseries, ti->extype->getFieldName(i)));
        if(ti->extype->getFieldName(i) == ti->fieldName) {
            primaryFieldNum = i;
            cout << "Primary Field number for extent type "<< ti->extype->getName()<< " and field name " << ti->fieldName << ": "<<primaryFieldNum << endl;
        }
    }
    assert(primaryFieldNum != -1);//, boost::format("Couldn't find a field matching %s") % ti->fieldName);

    uint64_t output_row_count = 0;

    std::map<int64_t, int> map_;
    for(int i = 0; i < sharedInfo->numPipelines ; ++i) {

        if(sourcePipes[i]->getFirstRecord()) {

            GeneralValue v(sourcePipes[i]->infields[primaryFieldNum]);
            int64_t record_id = v.valInt64();
            map_[record_id] = i;
        }
    }


    pthread_mutex_lock(&sharedInfo->mtx);
    pthread_cond_wait (&sharedInfo->masterDone, &sharedInfo->mtx);
    pthread_mutex_unlock(&sharedInfo->mtx);
    
    int64_t tmp_rec_id = INT64_MIN;
    for( std::map<int64_t, int>::iterator iter = map_.begin();iter != map_.end(); ) {

        int index = iter->second;
        GeneralValue v(sourcePipes[index]->infields[primaryFieldNum]);


        pthread_mutex_lock(&sharedInfo->mtx);

        if(v.valInt64() < tmp_rec_id) {
            std::cerr << "\33[0;31m" << "Extent type: " << ti->extype->getName() << " Current_rec_id = "<< v.valInt64() << " Last Record id was = "<< tmp_rec_id<< "\33[0m"<<endl;
            assert(sharedInfo->sharedVar < tmp_rec_id);
         }
        tmp_rec_id = v.valInt64();


        if(!sharedInfo->masterExited) {
            while(v.valInt64() > sharedInfo->sharedVar) {
                (sharedInfo->slaveDoneCnt)++;
                if(sharedInfo->slaveDoneCnt == sharedInfo->slaveThreadCnt) {

                    pthread_cond_signal(&sharedInfo->slaveDone);
                    sharedInfo->slaveDoneCnt = 0;

                }

				pthread_cond_wait (&sharedInfo->masterDone, &sharedInfo->mtx);
            }
        }
        pthread_mutex_unlock(&sharedInfo->mtx);


        if(v.valInt64() <= sharedInfo->sharedVar) {
            ti->outmodule->newRecord();
            ++output_row_count;

            for(unsigned int i=0; i< (sourcePipes[index]->infields).size(); ++i) {
                outfields[i]->set(sourcePipes[index]->infields[i]);
            }

            map_.erase(iter);

            if(sourcePipes[index]->getNextRecord()) {
                GeneralValue v(sourcePipes[index]->infields[primaryFieldNum]);
                int64_t record_id = v.valInt64();
                map_[record_id] = index;
            }
        }

        iter = map_.begin();

    }


    pthread_mutex_lock(&sharedInfo->mtx);
    if(!sharedInfo->masterExited) {
        (sharedInfo->slaveThreadCnt)--;
        if(sharedInfo->slaveDoneCnt == sharedInfo->slaveThreadCnt) {
            sharedInfo->slaveDoneCnt = 0;
            pthread_cond_signal(&sharedInfo->slaveDone);
        }
    }
    //cout << "extype: "<< ti->extype->getName()<< "slaveDoneCnt: "<< sharedInfo->slaveDoneCnt<< endl;
    //cout << "extype: "<< ti->extype->getName()<< "slaveThreadCnt: "<< sharedInfo->slaveThreadCnt<< endl;
    pthread_mutex_unlock(&sharedInfo->mtx);

    ti->outmodule->flushExtent();
    ti->outmodule->close();

    GeneralField::deleteFields(outfields);

    uint64_t total_input_rows = 0;
    for(int i = 0; i < sharedInfo->numPipelines ; ++i) {
        //cout << "Source " << i<< ": "<< sourcePipes[i]->input_row_count << "\n";
        total_input_rows += sourcePipes[i]->input_row_count;
    }
    cout << "Extent type: "<< ti->extype->getName()<< ", Total input rows: " << total_input_rows << ", Total Output rows: " << output_row_count<< "\n";
    return 0;
}

void * masterWorker(void *arg)
{
    TypeExtentMerge *ti = (TypeExtentMerge *)arg;
    DSMerge *sharedInfo = ti->dsMerge;
	uint64_t extent_num = 0;
	bool synced_with_others = false;


    SortedSourcePipe **sourcePipes = new SortedSourcePipe* [sharedInfo->numPipelines];

    for(int i = 0; i < sharedInfo->numPipelines ; ++i) {

        string subStr = sharedInfo->filePrefix + "_p" + boost::lexical_cast<std::string>(i) + "_";
        //source[i] = new TypeIndexModule(ti->extype->getName());
        sourcePipes[i] = new SortedSourcePipe(ti->extype);
        //        pipeline[i] = new SequenceModule(source[i]);

        for (vector<path>::iterator it(sharedInfo->vec->begin()), it_end(sharedInfo->vec->end()); it != it_end; it++) {
            if((*it).filename().generic_string().find(subStr) != string::npos)
                sourcePipes[i]->addSource((*it).generic_string());
        }
        sourcePipes[i]->startPrefetching();
    }

    vector<GeneralField *> outfields;

    int primaryFieldNum = -1;
    for(unsigned i = 0; i < ti->extype->getNFields(); ++i) {
        outfields.push_back(GeneralField::create(NULL, ti->outputseries, ti->extype->getFieldName(i)));
        if(ti->extype->getFieldName(i) == ti->fieldName) {
            primaryFieldNum = i;
            cout << "Primary Field number for extent type "<< ti->extype->getName()<< " and field name " << ti->fieldName << ": "<<primaryFieldNum << endl;
        }
    }
    INVARIANT(primaryFieldNum != -1, boost::format("Couldn't find a field matching %s") % ti->fieldName);

    uint64_t output_row_count = 0;

    std::map<int64_t, int> map_;
    for(int i = 0; i < sharedInfo->numPipelines ; ++i) {

        if(sourcePipes[i]->getFirstRecord()) {

            GeneralValue v(sourcePipes[i]->infields[primaryFieldNum]);
            int64_t record_id = v.valInt64();
            map_[record_id] = i;
        }
    }


    sleep(4);

    int64_t tmp_rec_id = INT64_MIN;;
    for( std::map<int64_t, int>::iterator iter = map_.begin();iter != map_.end(); ) {

        int index = iter->second;
        GeneralValue v(sourcePipes[index]->infields[primaryFieldNum]);
        sharedInfo->sharedVar = v.valInt64();
		GeneralValue ts_req(sourcePipes[index]->infields[0]); //trace::rpc request_at (nullable)
        GeneralValue ts_reply(sourcePipes[index]->infields[1]); //trace::rpc reply_at (nullable))
		sharedInfo->sharedTSVar = (ts_reply.valInt64() > ts_req.valInt64())? ts_reply.valInt64() : ts_req.valInt64(); //pick whichever is greater

        if(sharedInfo->sharedVar < tmp_rec_id) {
            std::cerr << "\33[0;31m" << "Extent type: " << ti->extype->getName() << " sharedVar = "<< sharedInfo->sharedVar << " Last Record id was = "<< tmp_rec_id<< "\33[0m"<<endl;
            assert(sharedInfo->sharedVar < tmp_rec_id);
        }
        tmp_rec_id = sharedInfo->sharedVar;

        ti->outmodule->newRecord();
        ++output_row_count;

        for(unsigned int i=0; i< (sourcePipes[index]->infields).size(); ++i) {
            outfields[i]->set(sourcePipes[index]->infields[i]);
        }

        map_.erase(iter);

        if(sourcePipes[index]->getNextRecord()) {
            GeneralValue v(sourcePipes[index]->infields[primaryFieldNum]);
            int64_t record_id = v.valInt64();
            map_[record_id] = index;
        }

        iter = map_.begin();

        if(sharedInfo->curFileOffset > sharedInfo->ROTATE_FILE_SIZE) {
            cout<< "\n--------------------\nRotating sinks....\n";
            //cout << "extype: "<< ti->extype->getName()<< "slaveDoneCnt: "<< sharedInfo->slaveDoneCnt<< endl;
            //cout << "extype: "<< ti->extype->getName()<< "slaveThreadCnt: "<< sharedInfo->slaveThreadCnt<< endl;
			pthread_mutex_lock(&sharedInfo->mtx);
			pthread_cond_broadcast(&sharedInfo->masterDone);
			pthread_cond_wait(&sharedInfo->slaveDone, &sharedInfo->mtx);
			pthread_mutex_unlock(&sharedInfo->mtx);
            sharedInfo->rotateSink();
        }

        if((ti->outmodule->curExtentSize() > (0.8 * ti->outmodule->getTargetExtentSize())) && !synced_with_others) {
            cout << boost::format("Processing extent #%d of size %d nearing the target size of %d\n") % extent_num++ % ti->outmodule->curExtentSize() % ti->outmodule->getTargetExtentSize();

            pthread_mutex_lock(&sharedInfo->mtx);
            pthread_cond_broadcast(&sharedInfo->masterDone);

			pthread_cond_wait(&sharedInfo->slaveDone, &sharedInfo->mtx);
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

    uint64_t total_input_rows = 0;
    for(int i = 0; i < sharedInfo->numPipelines ; ++i) {
        total_input_rows += sourcePipes[i]->input_row_count;
    }
    cout << "Extent type: "<< ti->extype->getName()<< ", Total input rows: " << total_input_rows << ", Total Output rows: " << output_row_count<< "\n";

    return 0;
}
