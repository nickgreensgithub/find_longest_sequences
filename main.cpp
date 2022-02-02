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
        if (otherFastaEntry.sequence.find(this->sequence) != string::npos) {
            if(otherFastaEntry.sequenceLength > this->sequenceLength){
                return true;
            }
        }
        return false;
    }
};

vector<fastaEntry> ReadFromFastaFile(string path);
void WriteToFastaFile(string path, vector<fastaEntry> records);
void eraseWhereLongerSequenceExists(vector<fastaEntry> records);
vector<fastaEntry> filterRecords(vector<fastaEntry> records);
//void filterRecords(vector<fastaEntry> records);
void WriteToFastaFile(string path, vector<fastaEntry> records);
vector<vector<fastaEntry*>> SplitVector(vector<fastaEntry>& records, int n = 1);

vector<fastaEntry*> GetPointersForRange(vector<fastaEntry>::iterator& startItem, long count);

int main(int argc, char* argv[])
{
    auto start = chrono::high_resolution_clock::now();
    auto records = ReadFromFastaFile("/mnt/files/projects/aau/sequence_matcher/top10k.fa");
    //auto records = ReadFromFastaFile("/mnt/files/projects/aau/sequence_matcher/test_dataset.fa");
    auto filtered = filterRecords(records);
    WriteToFastaFile("/mnt/files/projects/aau/sequence_matcher/output_dataset.fa", filtered);
    auto stop = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::seconds>(stop - start);
    cout << duration.count() << endl;
    return 0;
}

static bool DoesLongerSequenceExist(const fastaEntry* thisRecord, vector<fastaEntry>* records){
    for(auto record : *records){
        if (thisRecord->IsShorterVersionOf(record)){
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

//vector<fastaEntry> GetPointersForRange(){
//
//}

vector<fastaEntry*> GetPointersForRange(vector<fastaEntry>::iterator& startItem, long count) {
    auto endItem =  startItem+ count;
    vector<fastaEntry *> result;
    for (auto begin = startItem, end = endItem; begin != end; ++begin){
        result.push_back(&(*begin));
    }
    return result;
}

vector<vector<fastaEntry*>> SplitVector(vector<fastaEntry>& records, int n) {
    vector<vector<fastaEntry *>> chunks{};
    auto fullSize = records.size();
    auto itr = records.begin();
    for (int k = 0; k < n; ++k) {
        long chunkSize = fullSize / (n - k);
        fullSize -= chunkSize;
        chunks.push_back(GetPointersForRange(itr, chunkSize));
        itr += chunkSize;
    }
    return chunks;
}

vector<fastaEntry> filterRecords(vector<fastaEntry> records){
    auto chunkedInput = SplitVector(records);

    #pragma omp parallel for
    for(auto& chunk : chunkedInput) {
        chunk.erase(remove_if(chunk.begin(), chunk.end(), [&records](fastaEntry* record){
            return DoesLongerSequenceExist(record, &records);
        }));

//        chunk.erase( std::remove_if( chunk.begin(), chunk.end(), [&records](fastaEntry* record){
//            return DoesLongerSequenceExist(record, &records);
//        } ), chunk.end());
    }

    vector<fastaEntry> result;
    for(auto chunk : chunkedInput) {
        for(auto record: chunk){
            result.push_back(*record);
        }
    }
    return result;
}

vector<fastaEntry> ReadFromFastaFile(string path){
    vector<fastaEntry> fileContent;
    ifstream inputFile;
    inputFile.open(path, ios_base::in);
    if(inputFile.is_open()){
        string tempLine;
        string tempBuffer;
        while(getline(inputFile, tempLine)){
            if (tempLine.find(">", 0) == 0){
                //Get line until > or end of file
                //!TODO problem is this is taking away the delimiter from the next record
                getline(inputFile, tempBuffer, '>');
                tempBuffer.erase(std::remove(tempBuffer.begin(), tempBuffer.end(), '\n'), tempBuffer.end());
                fastaEntry newRecord = {
                        tempLine,
                        tempBuffer
                };
                fileContent.push_back(newRecord);
                if(!inputFile.eof()){
                    inputFile.putback('>');
                }

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
            //outputFile << records[i].header << endl;
            outputFile << ">FLASV" << i+1 << '.' << records[i].sequenceLength << endl;
            outputFile << records[i].sequence << endl;
        }
    }
}