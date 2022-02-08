//
// Created by nick on 1/02/22.
//
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <utility>
#include <vector>
#include <chrono>
#include <iterator>
#include <map>
#include "include/progressbar.h"

using namespace std;




struct fastaEntry{
    string header;
    string sequence;
    int sequenceLength;

    public:
        bool removed = false;

    fastaEntry(const string& header, const string& sequence){
        this->header = std::move(header);
        this->sequence = sequence;
        sequenceLength = sequence.length();
    }

    fastaEntry(){
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
vector<fastaEntry> SORTED_RECORDS;
map<int,int> SEARCH_INDEX;

vector<fastaEntry> ReadFromFastaFile(string path);
void WriteToFastaFile(string path, vector<fastaEntry> &records);
vector<fastaEntry> filterRecords(vector<fastaEntry> &records, vector<fastaEntry> &sortedRecords);
vector<vector<fastaEntry>> SplitVectorIntoChunks(vector<fastaEntry> &records, int n = THREADS);
vector<fastaEntry> GetChunk(vector<fastaEntry>::iterator startItem, long count);
map<int, int>  CreateLengthIndex(vector<fastaEntry> &records);
int CheckArgumentCount(int argc);

bool sortFunction (const fastaEntry& i, const fastaEntry& j) {
    return (i.sequenceLength<j.sequenceLength);
}

map<int, int> CreateLengthIndex(vector<fastaEntry> &records){
    map<int, int> lengths{};
    for (auto i = 0; i < records.size(); i++) {
        lengths.try_emplace(records[i].sequenceLength, i);
    }
    return lengths;
}

int main(int argc, char* argv[])
{
    auto countStatus = CheckArgumentCount(argc);
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
    cout << "Reading input file..." << endl;
    auto records = ReadFromFastaFile(inputPath);
    cout << "done" << endl;

    auto initialRecordNum = records.size();
    auto startPos = records.begin();
    SORTED_RECORDS = GetChunk(startPos, (long) initialRecordNum);

    cout << "Sorting records by length..." << endl;
    sort(SORTED_RECORDS.begin(), SORTED_RECORDS.end(), sortFunction);
    cout << "done" << endl;

    cout << "Creating length index..." << endl;
    SEARCH_INDEX = CreateLengthIndex(SORTED_RECORDS);
    cout << "done" << endl;

    cout << "Original record count: " << initialRecordNum << endl;
    auto filtered = filterRecords(records, SORTED_RECORDS);
    cout << endl << "Final record count: "<< filtered.size() << endl;
    WriteToFastaFile(outputPath, filtered);

    auto stop = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::seconds>(stop - start);
    cout << "Process took: " << duration.count() << " seconds to complete" <<endl;

    return 0;
}

int CheckArgumentCount(int argc){
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

static bool DoesLongerSequenceExist(const fastaEntry& thisRecord, const vector<fastaEntry>& records){//records are sorted by length
    auto startIndex = SEARCH_INDEX[thisRecord.sequenceLength];

    return any_of((records.begin() + startIndex), records.end(),[&thisRecord](const fastaEntry &item){
        return thisRecord.IsShorterVersionOf(item);
    });
}

vector<fastaEntry> GetChunk(vector<fastaEntry>::iterator startItem, long count){
    auto endItem =  startItem + count;
    vector<fastaEntry> result;
    for (auto begin = startItem, end = endItem; begin != end; ++begin){
        result.push_back(begin.operator*());
    }
    return result;
}

vector<vector<fastaEntry>> SplitVectorIntoChunks(vector<fastaEntry>& records, int n) {
    vector<vector<fastaEntry>> chunks{};
    auto fullSize = records.size();
    vector<fastaEntry>::iterator itr = records.begin();
    for (int k = 0; k < n; ++k) {
        long chunkSize = fullSize / (n - k);
        fullSize -= chunkSize;
        chunks.emplace_back(GetChunk(itr, chunkSize));
        itr += chunkSize;
    }
    return chunks;
}

vector<fastaEntry> filterRecords(vector<fastaEntry> &records, vector<fastaEntry> &sortedRecords){
    auto chunkedInput = SplitVectorIntoChunks(records);//records are in original order
    //Make progress bar
    progressbar bar(records.size());
    bar.set_todo_char(" ");
    bar.set_done_char("█");
    bar.set_opening_bracket_char("{");
    bar.set_closing_bracket_char("}");

    #pragma omp parallel for num_threads(THREADS)
    for(auto &chunk : chunkedInput) {
        chunk.erase(remove_if(chunk.begin(), chunk.end(), [&sortedRecords, &bar](const fastaEntry &record)->bool{

            //Update the progress bar
            #pragma omp critical
            bar.update();

            return DoesLongerSequenceExist(record, sortedRecords);
        }),chunk.end());
    }

    vector<fastaEntry> result;
    for(auto &chunk : chunkedInput) {
        for(auto const &record: chunk){
            result.push_back(record);
        }
    }
    return result;
}

vector<fastaEntry> ReadFromFastaFile(string path){
    vector<fastaEntry> fileContent;
    ifstream inputFile;

    inputFile.open(path, ios_base::in);
    if(!inputFile.is_open()){
        cout << "Input file cannot be opened, stopping."<< endl;
        throw "Input error";
    }

    if(inputFile.is_open()){
        string tempLine;
        string tempBuffer;
        while(getline(inputFile, tempLine)){
            if (tempLine.find('>', 0) == 0){
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

void WriteToFastaFile(string path, vector<fastaEntry>& records){
    ofstream outputFile;
    outputFile.open(path, ios_base::out | ios_base::trunc);
    if(outputFile.is_open()){
        //write the content
        for(auto & record : records) {
            outputFile << record.header << endl;
            //outputFile << ">FLASV" << i+1 << '.' << records[i].sequenceLength << endl;
            outputFile << record.sequence << endl;
        }
    }
}