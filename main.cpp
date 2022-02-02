//
// Created by nick on 1/02/22.
//

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <vector>
#include <chrono>

using namespace std;

struct fastaEntry{
    string header;
    string sequence;
    int sequenceLength;

    fastaEntry(string header, string sequence){
        this->header = header;
        this->sequence = sequence;
        sequenceLength = sequence.length();
    }

    operator const string(){
        ostringstream fastaStream;
        fastaStream << header << endl << sequenceLength << endl;
        return fastaStream.str();
    }

    bool IsShorterVersionOf (const fastaEntry& otherFastaEntry) const {
        if (this->sequence.find(otherFastaEntry.sequence) != string::npos) {
            if(otherFastaEntry.sequenceLength > this->sequenceLength){
                return true;
            }
        }
        return false;
        // Edge case not accounted for where 2 sequences are exactly the same
    }
};

vector<fastaEntry> ReadFromFastaFile(string path);
void WriteToFastaFile(string path, vector<fastaEntry> records);
void eraseWhereLongerSequenceExists(vector<fastaEntry> records);
vector<fastaEntry> filterRecords(vector<fastaEntry> records);
//void filterRecords(vector<fastaEntry> records);
void WriteToFastaFile(string path, vector<fastaEntry> records);
vector<vector<fastaEntry>> SplitVector(vector<fastaEntry> records, int n = 10);

int main(int argc, char* argv[])
{
    auto start = chrono::high_resolution_clock::now();
    //auto records = ReadFromFastaFile("/mnt/files/projects/aau/sequence_matcher/top10k.fa");
    auto records = ReadFromFastaFile("/mnt/files/projects/aau/sequence_matcher/test_dataset.fa");
    filterRecords(records);
    WriteToFastaFile("/mnt/files/projects/aau/sequence_matcher/output_dataset.fa", records);
    auto stop = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::seconds>(stop - start);
    cout << duration.count() << endl;
    return 0;
}

static bool DoesLongerSequenceExist(const fastaEntry thisRecord, vector<fastaEntry> records){
    for(auto record : records){
        if (thisRecord.IsShorterVersionOf(record)){
            return true;
        }
    }
    return false;
}

//vector<vector<fastaEntry>> SplitVector(vector<fastaEntry> records, int n){
//    int size = (records.size() - 1) / n + 1;
//    vector<vector<fastaEntry>> chunks;
//    for (int k = 0; k < size; ++k) {
//        auto start_itr = std::next(records.cbegin(), k*n);
//        auto end_itr = std::next(records.cbegin(), k*n + n);
//        vector<fastaEntry> chunk(n);
//        //chunks[k].resize(n);
//        if (k*n + n > records.size())
//        {
//            end_itr = records.cend();
////chunk.resize(records.size() - k*n);
//        }
//        copy(start_itr, end_itr, chunk.begin());
//        chunks.push_back(chunk);
//    }
//    return chunks;
//}

vector<vector<fastaEntry>> SplitVector(vector<fastaEntry> records, int n){
    vector<vector<fastaEntry>> chunks{};
    auto fullSize = records.size();
    auto itr = records.begin();
    for (int k = 0; k < n; ++k) {
        auto chunkSize = fullSize / (n - k);
        fullSize -= chunkSize;
        //TODO make the chunks use pointers to the original fastaEntry objects instead of copying them,should reduce ram overhead
        chunks.emplace_back(vector<fastaEntry>{itr, itr+chunkSize});
        itr += chunkSize;
    }
    return chunks;


//    // determine the total number of sub-vectors of size `n`
//    int size = (records.size() - 1) / n + 1;
//
//    // create an array of vectors to store the sub-vectors
//    std::vector<fastaEntry> vec[size];
//
//    // each iteration of this loop process the next set of `n` elements
//    // and store it in a vector at k'th index in `vec`
//    for (int k = 0; k < size; ++k)
//    {
//    // get range for the next set of `n` elements
//    auto start_itr = std::next(records.cbegin(), k*n);
//    auto end_itr = std::next(records.cbegin(), k*n + n);
//
//    // allocate memory for the sub-vector
//    vec[k].resize(n);
//
//    // code to handle the last sub-vector as it might
//    // contain fewer elements
//    if (k*n + n > records.size())
//    {
//    end_itr = records.cend();
//    vec[k].resize(records.size() - k*n);
//    }
//
//    // copy elements from the input range to the sub-vector
//    copy(start_itr, end_itr, vec[k].begin());
//
//    return vec;
}

//void filterRecords(vector<fastaEntry> records){
//    records.erase( std::remove_if( records.begin(), records.end(), [&records](fastaEntry& record){
//            return DoesLongerSequenceExist(record, records);
//        } ), records.end());
//}


vector<fastaEntry> filterRecords(vector<fastaEntry> records){
    auto chunkedInput = SplitVector(records);

    #pragma omp parallel for
    for(auto chunk : chunkedInput) {
        chunk.erase( std::remove_if( chunk.begin(), chunk.end(), [&records](fastaEntry& record){
            return DoesLongerSequenceExist(record, records);
        } ), chunk.end());
    }

    vector<fastaEntry> result;
    for(auto chunk : chunkedInput) {
        copy(chunk.begin(), chunk.end(), std::back_inserter(result));
    }
    return result;
}

vector<fastaEntry> ReadFromFastaFile(string path){
    vector<fastaEntry> fileContent;
    ifstream inputFile;
    inputFile.open(path, ios_base::in);
    if(inputFile.is_open()){
        string tempHeader;
        string tempSequence;
        while(getline(inputFile, tempHeader)){
            if (tempHeader.find(">", 0) == 0){
                //Get line until > or end of file
                getline(inputFile, tempSequence, '>');
                tempSequence.erase(std::remove(tempSequence.begin(), tempSequence.end(), '\n'), tempSequence.end());
                fastaEntry newRecord = {
                        tempHeader,
                        tempSequence
                };
                fileContent.push_back(newRecord);
            }
        }
        inputFile.close();
    }
    return fileContent;
}

void WriteToFastaFile(string path, vector<fastaEntry> records){
    ofstream outputFile;
    outputFile.open(path, ios_base::out | ios_base::trunc);
    if(outputFile.is_open()){
        //write the content
        for(int i = 0; i < records.size(); i ++) {
            outputFile << records[i].header << endl;
            outputFile << records[i].sequence << endl;
        }
    }
}