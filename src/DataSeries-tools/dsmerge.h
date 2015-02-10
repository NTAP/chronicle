#ifndef DSMERGE_H
#define DSMERGE_H

#include "DataSeries/GeneralField.hpp"
#include "DataSeries/TypeIndexModule.hpp"
#include "DataSeries/SequenceModule.hpp"
#include "DataSeries/ExtentSeries.hpp"

using namespace std;
using boost::format;

class SortedSourcePipe {

public:
    TypeIndexModule *source;
    ExtentSeries *inputseries; //(ExtentSeries::typeLoose);
    Extent::Ptr inextent;
    uint64_t input_row_count;


    vector<GeneralField *> infields;

    SortedSourcePipe(const ExtentType::Ptr extype) {
        input_row_count = 0;
        source = new TypeIndexModule(extype->getName());

        inputseries = new ExtentSeries(ExtentSeries::typeLoose);
        inputseries->setType(extype);

        for(unsigned i = 0; i < extype->getNFields(); ++i) {
            infields.push_back(GeneralField::create(NULL, *inputseries, extype->getFieldName(i)));
        }
    }

    ~SortedSourcePipe() {
        GeneralField::deleteFields(infields);
        delete inputseries;
        delete source;
    }

    void addSource(const std::string &filename) {
        INVARIANT(source != NULL, "source module not initialized");
        source->addSource(filename);
    }

    void startPrefetching(void) {
        source->startPrefetching( 32 * 1024 * 1024, 256 * 1024 * 1024, -1);
    }

    bool getFirstRecord() {

        inextent = source->getSharedExtent();
        if (inextent == NULL)
            return false;

        inputseries->setExtent(inextent);
        if(inputseries->morerecords()) {
         //   ++(*inputseries);
            ++input_row_count;
            return true;
        }
        else
            return false;
    }

    bool getNextRecord() {

        ++(*inputseries);
        while(true) {
            if(inputseries->morerecords()) {
         //       ++(*inputseries);
                ++input_row_count;
                return true;
            }
            else {
                inextent = source->getSharedExtent();
                if (inextent == NULL)
                    return false;
                else
                    inputseries->setExtent(inextent);
            }

        }
    }
};

#endif // DSMERGE_H
