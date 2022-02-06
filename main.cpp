//
// Created by nick on 1/02/22.
//
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <vector>
#include <chrono>
#include <iterator>

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
int THREADS;
vector<fastaEntry> ReadFromFastaFile(string path);
void WriteToFastaFile(string path, vector<fastaEntry> records);
void eraseWhereLongerSequenceExists(vector<fastaEntry> records);
vector<fastaEntry> filterRecords(vector<fastaEntry> records);
//void filterRecords(vector<fastaEntry> records);
void WriteToFastaFile(string path, vector<fastaEntry> records);
vector<vector<fastaEntry*>> SplitVector(vector<fastaEntry>& records, int n = THREADS);

vector<fastaEntry*> GetPointersForRange(vector<fastaEntry>::iterator& startItem, long count);
int CheckArgumentCount(int argc, char* argv[]);


int main(int argc, char* argv[])
{
    auto countStatus = CheckArgumentCount(argc, argv);
    if(countStatus > 0){
        return countStatus;
    }
    if(argc >= 4){
        int threads = stoi(argv[3]);
        THREADS = (threads > 0) ? threads : 1;
    }

    auto inputPath = argv[1];
    auto outputPath = argv[2];



    auto start = chrono::high_resolution_clock::now();
    auto records = ReadFromFastaFile(inputPath);
    cout << "original count: "<< records.size() << endl;


    auto filtered = filterRecords(records);
    cout << "reduced count: "<< filtered.size() << endl;
    WriteToFastaFile(outputPath, filtered);

    auto stop = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::seconds>(stop - start);
    cout << "Process took: " << duration.count() << " seconds to complete" <<endl;

    return 0;
}

int CheckArgumentCount(int argc, char* argv[]){
    int result = 0;
    if (argc < 2) {
        std::cerr << "missing input file" << endl;
        result =  EXIT_FAILURE;
    }
    if (argc < 3) {
        std::cerr << "missing output file" << endl;
        result =  EXIT_FAILURE;
    }
    if(result > 0){
        std::cerr << "usage: longest_sequence <input_file> <output_file> <threads>";
    }
    return result;
}

static bool DoesLongerSequenceExist(const fastaEntry* thisRecord, vector<fastaEntry>* records){
    for(auto record : *records){
        if (thisRecord->IsShorterVersionOf(record)){
            return true;
        }
    }
    return false;
}

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

    #pragma omp parallel for num_threads(THREADS)
    for(auto& chunk : chunkedInput) {
        chunk.erase(remove_if(chunk.begin(), chunk.end(), [&records](const fastaEntry* record){
            return DoesLongerSequenceExist(record, &records);
        }),chunk.end());
//        for(auto item : chunk){
//            cout << item->header <<endl;
//        }
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
            outputFile << records[i].header << endl;
            //outputFile << ">FLASV" << i+1 << '.' << records[i].sequenceLength << endl;
            outputFile << records[i].sequence << endl;
        }
    }
}